
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

using namespace llvm;
using namespace std;

using PointsToSet = map<string, set<string>>;

void initializeDataFlowMaps(Function &F, map<Instruction *, PointsToSet> &dfin,
                            map<Instruction *, PointsToSet> &dfout,
                            set<string> &allocatedPointers) {
  for (auto &basicblock : F) {
    for (auto &instn : basicblock) {
      dfin[&instn] = {};
      dfout[&instn] = {};

      if (auto *allocaInst = dyn_cast<AllocaInst>(&instn)) {
        if (allocaInst->getAllocatedType()->isPointerTy()) {
          allocatedPointers.insert(allocaInst->getNameOrAsOperand());
        }
      }
    }
  }
}

void aliasingFormalParametersOfFunction(
    Function &F, map<Instruction *, PointsToSet> &dfin,
    map<Instruction *, PointsToSet> &dfout) {
  vector<AllocaInst *> paramAddrs;
  BasicBlock &entry = F.getEntryBlock();

  for (auto &inst : entry) {
    if (auto *alloca = dyn_cast<AllocaInst>(&inst)) {
      if (alloca->getAllocatedType()->isPointerTy()) {
        StringRef name = alloca->getNameOrAsOperand();
        if (name.endswith(".addr")) {
          paramAddrs.push_back(alloca);
        }
      }
    }
  }

  Instruction *firstInst = &entry.front();

  for (size_t i = 0; i < paramAddrs.size(); i++) {
    for (size_t j = i + 1; j < paramAddrs.size(); j++) {
      string param1 = paramAddrs[i]->getNameOrAsOperand();
      string param2 = paramAddrs[j]->getNameOrAsOperand();

      dfin[firstInst][param1].insert("temp_");
      dfin[firstInst][param2].insert("temp_");
    }
  }
}

PointsToSet meetOfPredecessorMaps(const PointsToSet &currentBlockIn,
                                  const PointsToSet &predecessorOut) {
  PointsToSet mergedPointsToSet = currentBlockIn;

  for (const auto &entry : predecessorOut) {
    const string &ptrVar = entry.first;
    const set<string> &predecessorPointsToSet = entry.second;

    mergedPointsToSet[ptrVar].insert(predecessorPointsToSet.begin(),
                                     predecessorPointsToSet.end());
  }

  return mergedPointsToSet;
}

PointsToSet flowFunction(Instruction &I, PointsToSet &currentState,
                         set<string> &allocatedPointers) {

  if (auto *storeInst = dyn_cast<StoreInst>(&I)) {
    Value *storedValue = storeInst->getValueOperand();
    Value *pointerOperand = storeInst->getPointerOperand();

    if (storedValue->getType()->isPointerTy() &&
        pointerOperand->getType()->isPointerTy()) {
      string pointerOperandName = pointerOperand->getNameOrAsOperand();
      string storedValueName = storedValue->getNameOrAsOperand();
      if (allocatedPointers.find(pointerOperandName) ==
          allocatedPointers.end()) {

        if (currentState.find(pointerOperandName) != currentState.end()) {
          set<string> tempSet = currentState[pointerOperandName];
          for (const string &elem : tempSet) {

            if (allocatedPointers.find(storedValueName) !=
                allocatedPointers.end()) {
              currentState[elem].insert(storedValueName);
            } else {

              if (currentState.find(storedValueName) != currentState.end()) {
                for (const string &subElem : currentState[storedValueName]) {
                  currentState[elem].insert(subElem);
                }
              } else {
                  currentState[elem].clear();
                  currentState[elem] = {storedValueName};

              }
            }
          }
        }

      } else {
        currentState[pointerOperandName].clear();
        if (allocatedPointers.find(storedValueName) ==
            allocatedPointers.end()) {
          if (currentState.find(storedValueName) != currentState.end()) {
            for (const string &elem : currentState[storedValueName]) {
              currentState[pointerOperandName].insert(elem);
            }
          } else {
            currentState[pointerOperandName].insert(storedValueName);
          }

        } else {
          currentState[pointerOperandName].insert(storedValueName);
        }
      }
    }
  }

  else if (auto *loadInst = dyn_cast<LoadInst>(&I)) {
    Value *pointerOperand = loadInst->getPointerOperand();

    if (pointerOperand->getType()->isPointerTy()) {
      string pointerOperandName = pointerOperand->getNameOrAsOperand();
      string loadResultName = I.getNameOrAsOperand();

      if (currentState.find(pointerOperandName) != currentState.end() &&
          allocatedPointers.find(pointerOperandName) !=
              allocatedPointers.end()) {
        for (const string &pointedToPtr : currentState[pointerOperandName]) {
          currentState[loadResultName].insert(pointedToPtr);
        }
      } else {
        set<string> tempSet1 = currentState[pointerOperandName];
        for (const string &elem : tempSet1) {
          currentState[loadResultName].insert(currentState[elem].begin(),
                                              currentState[elem].end());
        }
      }
    }
  }

  else if (auto *gepInst = dyn_cast<GetElementPtrInst>(&I)) {
    string basePtr = gepInst->getPointerOperand()->getNameOrAsOperand();
    string resultPtr = gepInst->getNameOrAsOperand();
    if (!basePtr.empty() && !resultPtr.empty()) {
      currentState[resultPtr].insert(basePtr);
    }
  }

  else if (auto *callInst = dyn_cast<CallInst>(&I)) {
    string retName = callInst->getNameOrAsOperand();
    if (!retName.empty() && callInst->getType()->isPointerTy()) {
      currentState[retName] = {};
    }
  }

  else if (auto *bitcastInst = dyn_cast<BitCastInst>(&I)) {
    string basePtr = bitcastInst->getOperand(0)->getNameOrAsOperand();
    string resultPtr = bitcastInst->getNameOrAsOperand();

    if (!basePtr.empty() && !resultPtr.empty()) {

      currentState[resultPtr] = currentState[basePtr];
    }
  }

  return currentState;
}

void basicBlockWorklist(Function &F, map<Instruction *, PointsToSet> &dfin,
                        map<Instruction *, PointsToSet> &dfout,
                        set<string> &allocatedPointers) {

  list<BasicBlock *> worklist;
  for (BasicBlock &BB : F) {
    worklist.push_back(&BB);
  }

  while (!worklist.empty()) {
    BasicBlock *currentBlock = worklist.front();
    worklist.pop_front();

    PointsToSet currentBlockIn;
    bool firstPredecessor = true;

    for (BasicBlock *predecessorBlock : predecessors(currentBlock)) {
      Instruction *lastInst = nullptr;
      for (Instruction &I : *predecessorBlock) {
        lastInst = &I;
      }

      if (lastInst) {
        auto predecessorOut = dfout[lastInst];
        if (firstPredecessor) {
          currentBlockIn = predecessorOut;
          firstPredecessor = false;
        } else {

          currentBlockIn =
              meetOfPredecessorMaps(currentBlockIn, predecessorOut);
        }
      }
    }

    if (firstPredecessor && currentBlock == &F.getEntryBlock()) {
      currentBlockIn = PointsToSet();
    }

    bool changed = false;
    PointsToSet currentState = currentBlockIn;

    for (Instruction &I : *currentBlock) {

      dfin[&I] = currentState;

      PointsToSet newState = flowFunction(I, currentState, allocatedPointers);

      if (dfout.find(&I) == dfout.end() || newState != dfout[&I]) {
        changed = true;
        dfout[&I] = newState;
      }

      currentState = newState;
    }

    if (changed) {
      for (BasicBlock *successorBlock : successors(currentBlock)) {
        if (find(worklist.begin(), worklist.end(), successorBlock) ==
            worklist.end()) {
          worklist.push_back(successorBlock);
        }
      }
    }
  }
}

map<string, set<string>> computeAliasMapping(const PointsToSet &ptMap) {
  map<string, set<string>> aliasMapping;
  for (auto it1 = ptMap.begin(); it1 != ptMap.end(); ++it1) {
    for (auto it2 = ptMap.begin(); it2 != ptMap.end(); ++it2) {
      if (it1->first == it2->first)
        continue;

      set<string> intersection;
      for (const auto &elem : it1->second) {
        if (it2->second.count(elem) > 0) {
          intersection.insert(elem);
        }
      }
      if (!intersection.empty()) {
        aliasMapping[it1->first].insert(it2->first);
      }
    }
  }
  return aliasMapping;
}

set<string> expandRecursively(const string &key, const PointsToSet &ptMap,
                              set<string> &visited) {
  set<string> result;

  if (visited.find(key) != visited.end())
    return result;

  visited.insert(key);

  auto it = ptMap.find(key);
  if (it != ptMap.end()) {
    for (const auto &elem : it->second) {
      result.insert(elem);

      set<string> subExpansion = expandRecursively(elem, ptMap, visited);
      result.insert(subExpansion.begin(), subExpansion.end());
    }
  }

  return result;
}

void expandAllPointsTo(PointsToSet &ptMap) {
  for (auto &entry : ptMap) {
    set<string> visited;
    set<string> expanded = expandRecursively(entry.first, ptMap, visited);
    entry.second.insert(expanded.begin(), expanded.end());
  }
}

string stripAddrSuffix(const string &s) {
  string result = s;
  string suffix = ".addr";
  if (result.size() >= suffix.size() &&
      result.compare(result.size() - suffix.size(), suffix.size(), suffix) ==
          0) {
    result.erase(result.size() - suffix.size(), suffix.size());
  }
  return result;
}

void printAliasMappingInOrder(Function &F,
                              const map<string, set<string>> &aliasMapping,
                              const set<string> &allocatedPointers) {
  Module *M = F.getParent();
  if (!M) {
    cerr << "Error: Could not retrieve module information.\n";
    return;
  }

  string inputFilePath = M->getSourceFileName();
  if (inputFilePath.empty()) {
    cerr << "Error: Source file name is empty.\n";
    return;
  }

  size_t lastSlash = inputFilePath.find_last_of("/\\");
  string outputPath =
      (lastSlash == string::npos)
          ? "output.txt"
          : inputFilePath.substr(0, lastSlash + 1) + "output.txt";

  ofstream outputFile(outputPath.c_str(), ios_base::app);
  if (!outputFile.is_open()) {
    cerr << "Error: Could not open " << outputPath << " for writing.\n";
    return;
  }

  outputFile << "Function: " << F.getName().str() << "\n";

  vector<string> orderedPointers;
  BasicBlock &entry = F.getEntryBlock();
  for (Instruction &I : entry) {
    if (auto *allocaInst = dyn_cast<AllocaInst>(&I)) {
      if (allocaInst->getAllocatedType()->isPointerTy()) {
        string name = allocaInst->getNameOrAsOperand();
        if (allocatedPointers.find(name) != allocatedPointers.end())
          orderedPointers.push_back(name);
      }
    }
  }

  for (const string &ptrName : orderedPointers) {
    if (allocatedPointers.find(ptrName) == allocatedPointers.end())
      continue;

    string strippedPtrName = stripAddrSuffix(ptrName);

    outputFile << strippedPtrName << " -> {";

    auto it = aliasMapping.find(ptrName);
    vector<string> validAliases;
    if (it != aliasMapping.end()) {
      for (const string &alias : it->second) {
        if (allocatedPointers.find(alias) != allocatedPointers.end()) {
          validAliases.push_back(alias);
        }
      }
    }

    for (size_t i = 0; i < validAliases.size(); i++) {

      outputFile << stripAddrSuffix(validAliases[i]);
      if (i != validAliases.size() - 1)
        outputFile << ", ";
    }
    outputFile << "}\n";
  }

  outputFile << "\n";
  outputFile.close();
}

void processFinalAliasAnalysis(Function &F,
                               map<Instruction *, PointsToSet> &dfout,
                               set<string> &allocatedPointers) {
  if (F.empty()) return;  
  
  
  BasicBlock &lastBlock = F.back();
  Instruction *lastInst = lastBlock.getTerminator();
  if (!lastInst) return;
  PointsToSet &mapForInst = dfout[lastInst];
  expandAllPointsTo(mapForInst);  
  map<string, set<string>> aliasMapping = computeAliasMapping(mapForInst);
  printAliasMappingInOrder(F, aliasMapping, allocatedPointers);
}


void iteratingBasicBlock(Function &F) {
  map<Instruction *, PointsToSet> dfin;
  map<Instruction *, PointsToSet> dfout;
  set<string> allocatedPointers;
  initializeDataFlowMaps(F, dfin, dfout, allocatedPointers);
  aliasingFormalParametersOfFunction(F, dfin, dfout);
  basicBlockWorklist(F, dfin, dfout, allocatedPointers);
  processFinalAliasAnalysis(F, dfout, allocatedPointers);
}

namespace {
struct alias_c : public FunctionPass {
  static char ID;
  alias_c() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override {
    iteratingBasicBlock(F);
    return false;
  }
};
} // namespace

char alias_c::ID = 0;
static RegisterPass<alias_c> X("alias_lib_given",
                               "Alias Analysis Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);
