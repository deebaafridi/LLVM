#include <stdio.h>

void test1(){
  int a=10,b=11;
  int *p,*y;

  p=&a;
  y=&b;


  /*
    Alias information as observed at the last program point in function test1:
    p -> {}
    y -> {}
  
  
  */   

}

void test2(){
  int a=10,b=11;
  int *p,*y;

  if(b%2){
    p=&a;
  }
  else {
    p=&b; 	
  }
  y=&a;


  /*
    Alias information as observed at the last program point in function test2:
    p -> {y}
    y -> {p}
  
  
  */   

}

void test3(){
  int *p,*y,a,b,**p1;

  p=&a;
  y=&b;
  p1=&p;
  *p1=&b;

  /*
    Alias information as observed at the last program point in function test3:
    p -> {y, p1}
    y -> {p, p1}
    p1 -> {p, y}
  
  
  */   

}

void test4(){
  int x = 10, y = 20;
  int *p = &x;
  int *q = &y;
 
  if (x > 5) {
    q = p;
  }

  *p = 30;

  /*
    Alias information as observed at the last program point in function test4:
    p -> {q}
    q -> {p}
  */ 
}

void test5(int *x, int *y)
{
  int a1 = 10, b1 = 20;
  int *c = &a1, *d = &b1;
  x = c;
  y = d;

  /*
    Alias information as observed at the last program point in function test5:
    x -> {c}
    y -> {d}
    c -> {x}
    d -> {y}
  */ 
}

void test6() {
    int arr[5] = {1, 2, 3, 4, 5};
    int a;
    int *p = &arr[0];
    int *q = &a;

    if (arr[0] == arr[1]) {
        q = &arr[1];
    }
    /*
    Alias information as observed at the last program point in function test6:
    p -> {q}
    q -> {p}
  */ 
}

void test7() {
    int x = 10, y = 20;
    int *p = &x;
    int *q = &y;

    for (int i = 0; i < 5; i++) {
        if (i == 2) {
            q = p;  
        }
        *p = *p + 1;  
        *q = *q - 1; 
    }

    printf("x: %d, y: %d\n", x, y);
    /*
    Alias information as observed at the last program point in function test7:
    p -> {q}
    q -> {p}
  */ 
}

void test8(int **r, int flag) {
    int a, b;
    int *p1 = &a;
    int *p2 = &b;

    if (flag == 1) {
        r = &p2;  
    } else {
        r = &p1; 
    }

    **r = 20;  
    *p1 = 30;  
    *p2 = 40;  
    /*
    Alias information as observed at the last program point in function test8:
    r -> {p1, p2}
    p1 -> {r}
    p2 -> {r}
  */ 
}

int main()
{
  int a = 5, b = 5;
  test1();
  test2();
  test3();
  test4();
  test5(&a, &b);
  test6();
  test7();
  int **r, flag=0;
  test8(r, flag);
}