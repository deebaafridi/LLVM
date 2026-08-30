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
#include <stdio.h>
#include <string.h>

using namespace llvm;
using namespace std;

enum dataFlowValueType { TOP, CONSTANT, BOTTOM };

struct dataFlowValue {
  dataFlowValueType state;
  int value;

  dataFlowValue() : state(TOP), value(9999) {}
  dataFlowValue(int val) : state(CONSTANT), value(val) {}
  dataFlowValue(bool bottomCheck) : state(BOTTOM), value(-9999) {}

  bool operator==(const dataFlowValue &other) const {
    return state == other.state && value == other.value;
  }
  bool operator!=(const dataFlowValue &other) const {
    return !(*this == other);
  }
};

void initializeDataFlowMaps(
    Function &F, map<Instruction *, map<string, dataFlowValue>> &dfin,
    map<Instruction *, map<string, dataFlowValue>> &dfout) {
  for (auto &basicblock : F) {
    for (auto &instn : basicblock) {
      dfin[&instn] = {};
      dfout[&instn] = {};
    }
  }
}

void addingActualParameterOfFunction(
    Function &F, map<Instruction *, map<string, dataFlowValue>> &dfin,
    map<Instruction *, map<string, dataFlowValue>> &dfout) {
  for (auto &arg : F.args()) {
    string paramName = arg.getName().str();
    for (auto &basicblock : F) {
      for (auto &instn : basicblock) {
        dfin[&instn][paramName] = dataFlowValue(false);
        dfout[&instn][paramName] = dataFlowValue(false);
        return;
      }
    }
  }
}

void printDataFlowResults(
    Function &F, map<Instruction *, map<string, dataFlowValue>> &dfout) {

  Module *M = F.getParent();
  string moduleID = M->getModuleIdentifier();
  string fileName = sys::path::filename(moduleID).str();
  string baseFileName = fileName;
  size_t extensionPos = baseFileName.rfind(".ll");

  if (extensionPos != string::npos)
    baseFileName = baseFileName.substr(0, extensionPos);

  SmallString<256> outputDir(moduleID);
  sys::path::remove_filename(outputDir);
  sys::path::remove_filename(outputDir);
  sys::path::append(outputDir, "output");

  sys::fs::create_directories(outputDir);

  SmallString<256> outputPath(outputDir);
  sys::path::append(outputPath, baseFileName + ".txt");

  error_code EC;
  raw_fd_ostream outputFile(outputPath, EC, sys::fs::OF_None);
  if (EC) {
    errs() << "Failed to open output file: " << EC.message() << "\n";
    return;
  }

  string returnTypeStr;
  raw_string_ostream returnTypeStream(returnTypeStr);
  F.getReturnType()->print(returnTypeStream);
  outputFile << "define dso_local " << returnTypeStr << " @" << F.getName()
             << "() #0 {\n";

  for (BasicBlock &BB : F) {
    outputFile << (BB.hasName() ? BB.getName() : "unnamed") << ":\n";

    for (Instruction &I : BB) {
      outputFile << "  ";

      string instStr;
      raw_string_ostream rso(instStr);
      I.print(rso);
      outputFile << rso.str();

      auto analysisIt = dfout.find(&I);
      if (analysisIt != dfout.end()) {
        outputFile << " --> ";

        vector<pair<string, dataFlowValue>> orderedVars;
        set<string> processedVars;

        string resultVar = I.getNameOrAsOperand();
        if (!resultVar.empty()) {
          auto varIt = analysisIt->second.find(resultVar);
          if (varIt != analysisIt->second.end()) {
            orderedVars.push_back(*varIt);
            processedVars.insert(resultVar);
          }
        }

        for (Use &U : I.operands()) {
          Value *V = U.get();
          string operandName;

          if (Instruction *instV = dyn_cast<Instruction>(V)) {
            operandName = instV->getNameOrAsOperand();
          } else {
            operandName = V->getName().str();
          }

          if (processedVars.find(operandName) == processedVars.end()) {
            auto varIt = analysisIt->second.find(operandName);
            if (varIt != analysisIt->second.end()) {
              orderedVars.push_back(*varIt);
              processedVars.insert(operandName);
            }
          }
        }

        bool first = true;
        for (const auto &var : orderedVars) {
          if (!first)
            outputFile << ", ";
          first = false;

          if (!var.first.empty() && var.first[0] == '%')
            outputFile << var.first;
          else
            outputFile << "%" << var.first;

          outputFile << "=";

          if (var.second.state == TOP)
            outputFile << "TOP";
          else if (var.second.state == BOTTOM)
            outputFile << "BOTTOM";
          else
            outputFile << var.second.value;
        }
      }
      outputFile << "\n";
    }
  }
  outputFile << "}\n";

  outputFile.flush();
  outputFile.close();
}

dataFlowValue meetOfPredecessorMaps(dataFlowValue mapValue,
                                    dataFlowValue incomingValue) {
  if (mapValue.state == TOP)
    return incomingValue;
  if (incomingValue.state == TOP)
    return mapValue;
  if (mapValue.state == BOTTOM || incomingValue.state == BOTTOM)
    return dataFlowValue(false);
  if (mapValue.state == CONSTANT && incomingValue.state == CONSTANT) {
    if (mapValue.value == incomingValue.value)
      return mapValue;
    else
      return dataFlowValue(false);
  }
  return dataFlowValue(false);
}

map<string, dataFlowValue>
flowFunction(Instruction &instn, map<string, dataFlowValue> &totalEffect) {

  map<string, dataFlowValue> newEffect = totalEffect;

  if (isa<AllocaInst>(&instn)) {
    string variable = instn.getNameOrAsOperand();
    if(newEffect.find(variable) != newEffect.end())
    {
      if(newEffect[variable].state == BOTTOM)
      {
        return newEffect;
      }
    }
    newEffect[variable] = dataFlowValue();
    return newEffect;
  }

  else if (isa<LoadInst>(&instn)) {
    string lhsvariable = instn.getNameOrAsOperand();
    if(newEffect.find(lhsvariable) != newEffect.end())
    {
      if(newEffect[lhsvariable].state == BOTTOM)
      {
        return newEffect;
      }
    }
    Value *pointerOperand = instn.getOperand(0);
    string operandName = pointerOperand->getNameOrAsOperand();
    newEffect[lhsvariable] = newEffect[operandName];
    return newEffect;
  }

  else if (isa<StoreInst>(&instn)) {
    auto *storeInst = dyn_cast<StoreInst>(&instn);
    Value *valOperand = storeInst->getValueOperand();
    Value *ptrOperand = storeInst->getPointerOperand();

    if (auto *ptrInst = dyn_cast<Instruction>(ptrOperand)) {
      string var = ptrInst->getNameOrAsOperand();
      if(newEffect.find(var) != newEffect.end())
    {
      if(newEffect[var].state == BOTTOM)
      {
        return newEffect;
      }
    }

      if (auto *CI = dyn_cast<ConstantInt>(valOperand)) {
        int value = static_cast<int>(CI->getSExtValue());
        newEffect[var] = dataFlowValue(value);
      }

      else {
        auto *valInst = dyn_cast<Instruction>(valOperand);
        string valName = valInst->getNameOrAsOperand();
        newEffect[var] = totalEffect[valName];
      }
    }
    return newEffect;
  }

  else if (auto *binOp = dyn_cast<BinaryOperator>(&instn)) {

    string var = instn.getNameOrAsOperand();
    if(newEffect.find(var) != newEffect.end())
    {
      if(newEffect[var].state == BOTTOM)
      {
        return newEffect;
      }
    }

    Value *lhs = binOp->getOperand(0);
    Value *rhs = binOp->getOperand(1);

    int lhsVal, rhsVal;
    dataFlowValueType lhstype, rhstype;

    if (auto *CI = dyn_cast<ConstantInt>(lhs)) {
      lhsVal = CI->getSExtValue();
      lhstype = CONSTANT;
    } else {
      lhsVal = newEffect[lhs->getNameOrAsOperand()].value;
      lhstype = newEffect[lhs->getNameOrAsOperand()].state;
    }

    if (auto *CI = dyn_cast<ConstantInt>(rhs)) {
      rhsVal = CI->getSExtValue();
      rhstype = CONSTANT;
    } else {
      rhsVal = newEffect[rhs->getNameOrAsOperand()].value;
      rhstype = newEffect[rhs->getNameOrAsOperand()].state;
    }

    if (lhstype == BOTTOM || rhstype == BOTTOM) {
      newEffect[var] = dataFlowValue(false);
    } else if (lhstype == TOP || rhstype == TOP) {
      newEffect[var] = dataFlowValue();
    } else {
      int computedResult = 0;

      switch (binOp->getOpcode()) {
      case Instruction::Add:
        computedResult = lhsVal + rhsVal;
        break;
      case Instruction::Sub:
        computedResult = lhsVal - rhsVal;
        break;
      case Instruction::Mul:
        computedResult = lhsVal * rhsVal;
        break;
      case Instruction::SDiv:
        if (rhsVal != 0) {
          computedResult = lhsVal / rhsVal;
        }
        break;
      case Instruction::SRem:
        if (rhsVal != 0) {
          computedResult = lhsVal % rhsVal;
        }
        break;
      case Instruction::And:
        computedResult = lhsVal & rhsVal;
        break;
      case Instruction::Or:
        computedResult = lhsVal | rhsVal;
        break;
      case Instruction::Xor:
        if (rhsVal == -1) {
          computedResult = ~lhsVal;
        } else {
          computedResult = lhsVal ^ rhsVal;
        }
        break;
      case Instruction::Shl:
        computedResult = lhsVal << rhsVal;
        break;
      case Instruction::LShr:
        computedResult = static_cast<uint32_t>(lhsVal) >> rhsVal;
        break;
      case Instruction::AShr:
        computedResult = lhsVal >> rhsVal;
        break;
      default:
        return newEffect;
      }

      newEffect[var] = dataFlowValue(computedResult);
    }
  }

  else if (dyn_cast<SelectInst>(&instn)) {

    string var = instn.getNameOrAsOperand();
    newEffect[var] = dataFlowValue(false);

  }

  else if (dyn_cast<GetElementPtrInst>(&instn)) {

    string ptrName = instn.getNameOrAsOperand();
    newEffect[ptrName] = dataFlowValue(false);
  }

  else if (auto *callInst = dyn_cast<CallInst>(&instn)) {

    Function *calledFunc = callInst->getCalledFunction();

    if (calledFunc) {
      string funcName = calledFunc->getName().str();
      string callVar = callInst->getNameOrAsOperand();

      if (funcName == "scanf" || funcName == "__isoc99_scanf") {

        if (callInst->getNumOperands() > 1) {
          Value *scannedVarPtr = callInst->getOperand(1);
          if (auto *scannedInst = dyn_cast<Instruction>(scannedVarPtr)) {
            string scannedVar = scannedInst->getNameOrAsOperand();
            newEffect[scannedVar] = dataFlowValue(false);
          }
        }

        newEffect[callVar] = dataFlowValue(false);
      } else {
        newEffect[callVar] = dataFlowValue(false);
      }
    }
  }

  return newEffect;
}

void basicBlockWorklist(Function &F,
                        map<Instruction *, map<string, dataFlowValue>> &dfin,
                        map<Instruction *, map<string, dataFlowValue>> &dfout) {
  list<BasicBlock *> worklist;

  worklist.push_back(&F.getEntryBlock());

  while (!worklist.empty()) {
    BasicBlock *BB = worklist.front();
    worklist.pop_front();

    map<string, dataFlowValue> totalEffect;

    if (BB != &F.getEntryBlock()) {
      for (BasicBlock *P : predecessors(BB)) {
        Instruction *lastInstn = nullptr;
        for (Instruction &instn : *P) {
          lastInstn = &instn;
        }
        if (lastInstn) {
          auto effect = dfout[lastInstn];
          for (auto &entry : effect) {
            string var = entry.first;
            dataFlowValue val = entry.second;
            if (totalEffect.find(var) == totalEffect.end()) {
              totalEffect[var] = val;
            } else {
              totalEffect[var] = meetOfPredecessorMaps(totalEffect[var], val);
            }
          }
        }
      }
    }

    bool changed = false;
    for (Instruction &instn : *BB) {
      dfin[&instn] = totalEffect;

      auto newEffect = flowFunction(instn, totalEffect);

      if (newEffect != dfout[&instn]) {
        dfout[&instn] = newEffect;
        changed = true;
      }
      totalEffect = newEffect;
    }

    if (changed) {
      for (BasicBlock *Succ : successors(BB)) {

        worklist.push_back(Succ);
      }
    }
  }
}

void iteratingBasicBlock(Function &F) {
  map<Instruction *, map<string, dataFlowValue>> dfin;
  map<Instruction *, map<string, dataFlowValue>> dfout;

  initializeDataFlowMaps(F, dfin, dfout);
  addingActualParameterOfFunction(F, dfin, dfout);
  basicBlockWorklist(F, dfin, dfout);
  printDataFlowResults(F, dfout);
}

namespace {
struct constant_p : public FunctionPass {
  static char ID;
  constant_p() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    iteratingBasicBlock(F);
    return false;
  }
}; // end of struct constant_p
} // end of anonymous namespace

char constant_p::ID = 0;
static RegisterPass<constant_p> X("libCP_given",
                                  "Constant Propagation Pass",
                                  false /* Only looks at CFG */,
                                  false /* Analysis Pass */);
