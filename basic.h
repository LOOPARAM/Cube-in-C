#ifndef BASIC_H
#define BASIC_H

#include <stdio.h>
#include <stdlib.h>

// 스택에 사용할 노드 구조체 정의 (포인터를 저장)
typedef struct Node {
    void* data;           // 포인터를 저장
    struct Node* next;    // 다음 노드를 가리킬 포인터
} Node;

// 스택 구조체 정의
typedef struct {
    Node* top;  // 스택의 top을 가리킬 포인터
    int size;   // 스택의 길이를 저장
} Stack;

// 스택 초기화 함수
void initStack(Stack* s) {
    s->top = NULL;  // 스택이 비어 있으면 top은 NULL
    s->size = 0;    // 스택의 길이를 0으로 초기화
}

// 스택이 비었는지 확인하는 함수
int isEmpty(Stack* s) {
    return s->top == NULL;
}

// 스택에 데이터를 삽입하는 함수 (push) - 포인터를 받음
void push(Stack* s, void* value) {
    // 새 노드를 동적 할당
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("Memory allocation failed!\n");
        return;
    }

    newNode->data = value;  // 노드의 데이터에 값을 저장 (포인터)
    newNode->next = s->top;  // 새 노드는 기존의 top을 가리킴
    s->top = newNode;        // 새 노드를 top으로 설정
    s->size++;               // 스택 길이 증가
}

// 스택에서 데이터를 제거하는 함수 (pop) - 포인터를 반환
void* pop(Stack* s) {
    if (isEmpty(s)) {
        printf("Stack underflow\n");
        return NULL;  // 스택이 비었을 때 NULL 반환
    } else {
        Node* temp = s->top;    // 현재 top 노드를 temp에 저장
        void* poppedValue = temp->data;  // top 노드의 데이터를 가져옴 (포인터)
        s->top = s->top->next;   // top을 다음 노드로 이동
        free(temp);              // 제거된 노드 메모리 해제
        s->size--;               // 스택 길이 감소
        return poppedValue;      // 제거된 값(포인터) 반환
    }
}

// 스택의 길이를 반환하는 함수
int getSize(Stack* s) {
    return s->size;
}

// 스택의 top에 있는 값을 확인하는 함수 (peek) - 포인터를 반환
void* peek(Stack* s) {
    if (isEmpty(s)) {
        printf("Stack is empty\n");
        return NULL;  // 스택이 비었을 때 NULL 반환
    } else {
        return s->top->data;  // top 노드의 데이터(포인터) 반환
    }
}

// 인덱스 기반 접근 함수: 0은 스택의 top, 1은 그 다음 요소 등
void* getAtIndex(Stack* s, int index) {
    if (index < 0 || index >= s->size) {
        printf("Index out of bounds\n");
        return NULL;
    }
    Node* current = s->top;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    return current->data;
}

#endif //BASIC_H