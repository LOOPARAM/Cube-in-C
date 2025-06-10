#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <thread>
#include "basic.h"
#include <time.h>


//화면의 가로, 세로 크기를 표현
#define x_size 100
#define y_size 60

//화면 FOV, d, 카메라 위치
#define FOV 60
#define FOV_RAD (FOV * M_PI / 180.0)  // 도를 라디안으로 변환
#define d (1.0 / tan(FOV_RAD / 2.0))
#define CAMERA_X (x_size / 2)
#define CAMERA_Y (y_size / 2)
#define CAMERA_Z -70

// 아웃코드 정의
#define INSIDE 0  // 0000
#define LEFT   1  // 0001
#define RIGHT  2  // 0010
#define BOTTOM 4  // 0100
#define TOP    8  // 1000

//점2D 구조체
struct Point2D {
    float x;
    float y;
};
//선2D 구조체
struct Line2D {
    Stack line;
};
//Vector3
struct Vector3 {
    float x;
    float y;
    float z;
};
struct Cube {
    Vector3 point[8];
};


//함수 미리 선언
int kbhit();
void initScreen();
void ShowScreen();
void moveCursor(int x, int y);
void clearConsole();
void update(Stack* obj, Stack* prev_obj);
void DelObj(Stack* obj);
int computeOutCode(Point2D *point);
bool clipLine(Point2D *p1, Point2D *p2, Point2D *clipped_p1, Point2D *clipped_p2);
void DrawLine(Point2D *p1, Point2D *p2, struct Line2D *line, struct Line2D *prev_line);
Point2D Vec3ToCamera(Vector3 *p);
void MoveCube(Cube* cube);
void RotateCube(Cube* cube, char axis, float angle, Line2D* cube_lines, Line2D* prev_cube_lines);

//화면 모든 픽셀값에 대한 정보
char screen[y_size][x_size] = {};

//키 입력 체크 -> 빡세서 gpt 시킴
int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

//화면 초기화 함수
void initScreen() {;
    for (int i = 0; i < y_size; i++) {
        for (int j = 0; j < x_size; j++) {
            if (j == 0 || i == 0 || j == x_size - 1 || i == y_size-1) {
                screen[i][j] = '*';
            }
            else {
                screen[i][j] = ' ';
            }
        }
    }
}

//화면 출력
void ShowScreen() {
    for (int i = 0; i < y_size; i++) {
        for (int j = 0; j < x_size; j++) {
            printf("%c ", screen[i][j]);
        }
        printf("\n");
    }
}

//커서 이동 함수
void moveCursor(int x, int y) {
    printf("\033[%d;%dH", y_size-y-1, x);
}

//콘솔 초기화!
void clearConsole()
{
    system("clear");
}

//없데이트 ㅋ
void update(Stack* obj, Stack* prev_obj) {
    int obj_size = getSize(obj);
    int prev_size = getSize(prev_obj);
    moveCursor(0, -1);
    //printf("%d / %d", obj_size, prev_size);

    // Clear previous line using the initial size
    for (int i = 0; i < prev_size; i++) {
        Point2D* pointPtr = (Point2D*)getAtIndex(prev_obj, i);
        if (pointPtr != NULL) {
            Point2D point = *pointPtr;
            moveCursor(round(point.x)*2, round(point.y));
            printf("  ");
        }
    }

    // Draw current line
    for (int i = 0; i < obj_size; i++) {
        Point2D* pointPtr = (Point2D*)getAtIndex(obj, i);
        if (pointPtr != NULL) {
            Point2D point = *pointPtr;
            moveCursor(round(point.x)*2, round(point.y));
            printf("* ");
        }
    }
}

//객체 제거하기
void DelObj(Stack* obj) {
    int prev_size = getSize(obj);
    for (int i = 0; i < prev_size; i++) {
        Point2D* pointPtr = (Point2D*)getAtIndex(obj, i);
        if (pointPtr != NULL) {
            Point2D point = *pointPtr;
            moveCursor(round(point.x)*2, round(point.y));
            printf("  ");
        }
    }
}

// 점의 아웃코드 계산 함수
int computeOutCode(Point2D *point) {
    int code = INSIDE;

    if (point->x < 0)
        code |= LEFT;
    else if (point->x >= x_size)
        code |= RIGHT;

    if (point->y < 0)
        code |= BOTTOM;
    else if (point->y >= y_size-2)
        code |= TOP;

    return code;
}

// Cohen-Sutherland 클리핑 알고리즘
bool clipLine(Point2D *p1, Point2D *p2, Point2D *clipped_p1, Point2D *clipped_p2) {
    // 원본 점들을 복사
    *clipped_p1 = *p1;
    *clipped_p2 = *p2;

    int outcode1 = computeOutCode(clipped_p1);
    int outcode2 = computeOutCode(clipped_p2);
    bool accept = false;

    while (true) {
        if (!(outcode1 | outcode2)) {
            // 두 점 모두 화면 안에 있음
            accept = true;
            break;
        } else if (outcode1 & outcode2) {
            // 두 점 모두 같은 영역 밖에 있음 (완전히 화면 밖)
            break;
        } else {
            // 적어도 한 점은 화면 밖에 있음
            float x, y;
            int outcodeOut = outcode1 ? outcode1 : outcode2;

            // 화면 경계와의 교점 계산
            if (outcodeOut & TOP) {           // 위쪽 경계
                x = clipped_p1->x + (clipped_p2->x - clipped_p1->x) * (y_size - 1 - clipped_p1->y) / (clipped_p2->y - clipped_p1->y);
                y = y_size - 3;
            } else if (outcodeOut & BOTTOM) { // 아래쪽 경계
                x = clipped_p1->x + (clipped_p2->x - clipped_p1->x) * (0 - clipped_p1->y) / (clipped_p2->y - clipped_p1->y);
                y = 0;
            } else if (outcodeOut & RIGHT) {  // 오른쪽 경계
                y = clipped_p1->y + (clipped_p2->y - clipped_p1->y) * (x_size - 1 - clipped_p1->x) / (clipped_p2->x - clipped_p1->x);
                x = x_size - 1;
            } else if (outcodeOut & LEFT) {   // 왼쪽 경계
                y = clipped_p1->y + (clipped_p2->y - clipped_p1->y) * (0 - clipped_p1->x) / (clipped_p2->x - clipped_p1->x);
                x = 0;
            }

            // 교점으로 점 업데이트
            if (outcodeOut == outcode1) {
                clipped_p1->x = x;
                clipped_p1->y = y;
                outcode1 = computeOutCode(clipped_p1);
            } else {
                clipped_p2->x = x;
                clipped_p2->y = y;
                outcode2 = computeOutCode(clipped_p2);
            }
        }
    }

    return accept;
}

// 수정된 DrawLine 함수
void DrawLine(Point2D *p1, Point2D *p2, struct Line2D *line, struct Line2D *prev_line) {
    Line2D duplicate_line = *line;
    initStack(&prev_line->line);
    *prev_line = duplicate_line;

    initStack(&line->line);

    // 클리핑된 점들
    Point2D clipped_p1, clipped_p2;

    // 선분 클리핑 수행
    if (!clipLine(p1, p2, &clipped_p1, &clipped_p2)) {
        // 선분이 완전히 화면 밖에 있으면 아무것도 그리지 않음
        return;
    }

    // 클리핑된 점들을 반올림
    int start_x = round(clipped_p1.x);
    int start_y = round(clipped_p1.y);
    int end_x = round(clipped_p2.x);
    int end_y = round(clipped_p2.y);
    if (start_x >= x_size-1) {
        start_x = x_size-2;
    }
    if (end_x >= x_size-1) {
        end_x = x_size-2;
    }
    if (start_x <= 0) {
        start_x = 1;
    }
    if (end_x <= 0) {
        end_x = 1;
    }
    if (start_y >= y_size-2) {
        start_y = y_size-3;
    }
    if (end_y >= y_size-2) {
        end_y = y_size-3;
    }


    // 나머지는 기존 브레젠험 알고리즘과 동일
    int width = end_x - start_x;
    int height = end_y - start_y;

    bool isGradualSlope = abs(width) > abs(height);
    int dx = (width < 0) ? -1 : 1;
    int dy = (height > 0) ? 1 : -1;

    int f = isGradualSlope ? dy * height - dx * width : 2 * dx * width - dy * height;
    int f1 = (isGradualSlope ? 2 * dy * height - dx : 2 * dx * width);
    int f2 = (isGradualSlope ? 2 * (dy * height - dx * width): -2 * (dy * height - dx * width));

    int x = start_x;
    int y = start_y;

    if (isGradualSlope) {
        while (x != end_x) {
            // 화면 경계 내부인지 한 번 더 확인
            if (x >= 0 && x < x_size && y >= 0 && y < y_size) {
                Point2D* point_ptr = (Point2D*)malloc(sizeof(Point2D));
                if (!point_ptr) {
                    fprintf(stderr, "Memory allocation failed\n");
                    exit(1);
                }
                point_ptr->x = (float)x;
                point_ptr->y = (float)y;
                push(&line->line, point_ptr);
            }

            if (f < 0) {
                f += f1;
            } else {
                f += f2;
                y += dy;
            }
            x += dx;
        }
    } else {
        while (y != end_y) {
            // 화면 경계 내부인지 한 번 더 확인
            if (x >= 0 && x < x_size && y >= 0 && y < y_size) {
                Point2D* point_ptr = (Point2D*)malloc(sizeof(Point2D));
                if (!point_ptr) {
                    fprintf(stderr, "Memory allocation failed\n");
                    exit(1);
                }
                point_ptr->x = (float)x;
                point_ptr->y = (float)y;
                push(&line->line, point_ptr);
            }

            if (f < 0) {
                f += f1;
            } else {
                f += f2;
                x += dx;
            }
            y += dy;
        }
    }

    // 마지막 점도 추가 (화면 경계 내부인 경우)
    if (x >= 0 && x < x_size && y >= 0 && y < y_size) {
        Point2D* point_ptr = (Point2D*)malloc(sizeof(Point2D));
        if (!point_ptr) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        point_ptr->x = (float)x;
        point_ptr->y = (float)y;
        push(&line->line, point_ptr);
    }
}

//원근 투영 변환
Point2D Vec3ToCamera(Vector3 *p) {
    Point2D PointInCamera = { 0, 0 };
    float x = p->x - CAMERA_X;
    float y = p->y - CAMERA_Y;
    float z = p->z - CAMERA_Z;

    // Z값이 0이거나 카메라 뒤쪽인 경우 처리
    if (z <= 0) {
        return PointInCamera;
    }

    // 표준 원근 투영
    PointInCamera.x = (x / z) * d;
    PointInCamera.y = (y / z) * d;  // 이 부분이 수정됨

    // 종횡비 보정 (보통 x에만 적용)
    float aspect_ratio = (float)y_size / x_size;
    PointInCamera.x *= aspect_ratio;

    // 화면 좌표로 변환
    PointInCamera.x = PointInCamera.x * (x_size/2) + (x_size/2);
    PointInCamera.y = PointInCamera.y * (y_size/2) + (y_size/2);

    return PointInCamera;
}

//큐브 회전 함수
void MoveCube(Cube* cube, char axis, int length, Line2D* cube_lines, Line2D* prev_cube_lines) {
    // 먼저 이전 큐브를 완전히 지우기
    for (int i = 0; i < 12; i++) {
        DelObj(&cube_lines[i].line);
    }

    // 큐브의 모든 점 이동
    for (int i = 0; i < 8; i++) {
        if (axis == 'x') {
            cube->point[i].x += length;
        }
        else if (axis == 'y') {
            cube->point[i].y += length;
        }
        else if (axis == 'z') {
            cube->point[i].z += length;
        }
    }

    // 새로운 2D 좌표로 변환
    Point2D new_point[8];
    for (int i = 0; i < 8; i++) {
        new_point[i] = Vec3ToCamera(&cube->point[i]);
    }

    // 모든 라인 스택 초기화
    for (int i = 0; i < 12; i++) {
        initStack(&cube_lines[i].line);
        initStack(&prev_cube_lines[i].line);
    }

    // 정육면체 모서리 연결 (12개의 모서리)
    // 앞면 (z = 20) 4개 모서리
    DrawLine(&new_point[0], &new_point[2], &cube_lines[0], &prev_cube_lines[0]);
    DrawLine(&new_point[2], &new_point[4], &cube_lines[1], &prev_cube_lines[1]);
    DrawLine(&new_point[4], &new_point[3], &cube_lines[2], &prev_cube_lines[2]);
    DrawLine(&new_point[3], &new_point[0], &cube_lines[3], &prev_cube_lines[3]);

    // 뒷면 (z = -20) 4개 모서리
    DrawLine(&new_point[1], &new_point[5], &cube_lines[4], &prev_cube_lines[4]);
    DrawLine(&new_point[5], &new_point[7], &cube_lines[5], &prev_cube_lines[5]);
    DrawLine(&new_point[7], &new_point[6], &cube_lines[6], &prev_cube_lines[6]);
    DrawLine(&new_point[6], &new_point[1], &cube_lines[7], &prev_cube_lines[7]);

    // 앞면과 뒷면을 연결하는 4개 모서리
    DrawLine(&new_point[0], &new_point[1], &cube_lines[8], &prev_cube_lines[8]);
    DrawLine(&new_point[2], &new_point[5], &cube_lines[9], &prev_cube_lines[9]);
    DrawLine(&new_point[4], &new_point[7], &cube_lines[10], &prev_cube_lines[10]);
    DrawLine(&new_point[3], &new_point[6], &cube_lines[11], &prev_cube_lines[11]);

    // 모든 라인 업데이트 (새로운 큐브 그리기)
    for (int i = 0; i < 12; i++) {
        int obj_size = getSize(&cube_lines[i].line);
        for (int j = 0; j < obj_size; j++) {
            Point2D* pointPtr = (Point2D*)getAtIndex(&cube_lines[i].line, j);
            if (pointPtr != NULL) {
                Point2D point = *pointPtr;
                moveCursor(round(point.x)*2, round(point.y));
                printf("* ");
            }
        }
    }
}

//큐브 회전 함수
void RotateCube(Cube* cube, char axis, float angle, Line2D* cube_lines, Line2D* prev_cube_lines) {

}

//main 함수
int main() {
    setbuf(stdout, NULL);
    //콘솔 청소 + 화면 배열 초기화 + 스택 초기화
    clearConsole();
    initScreen();
    ShowScreen();

    //직선 생성
    Point2D point1 = {x_size/2,y_size/2};
    Point2D point2 = {x_size/2 + 10,y_size/2};
    Line2D _line;
    Line2D prev_line;
    initStack(&_line.line);
    initStack(&prev_line.line);

    for (int i = 0; i < 10; i++) {
        DrawLine(&point1,&point2, &_line, &prev_line);
        update(&_line.line, &prev_line.line);
        usleep(10000);
        point2.y -= 1;
    }
    for (int i = 0; i < 20; i++) {
        DrawLine(&point1,&point2, &_line, &prev_line);
        update(&_line.line, &prev_line.line);
        usleep(10000);
        point2.x -= 1;
    }
    for (int i = 0; i < 20; i++) {
        DrawLine(&point1,&point2, &_line, &prev_line);
        update(&_line.line, &prev_line.line);
        usleep(10000);
        point2.y += 1;
    }
    for (int i = 0; i < 20; i++) {
        DrawLine(&point1,&point2, &_line, &prev_line);
        update(&_line.line, &prev_line.line);
        usleep(10000);
        point2.x += 1;
    }
    for (int i = 0; i < 10; i++) {
        DrawLine(&point1,&point2, &_line, &prev_line);
        update(&_line.line, &prev_line.line);
        usleep(10000);
        point2.y -= 1;
    }

    DelObj(&_line.line);


    //정육면체 렌더링
    Vector3 Point1 = {x_size/2+20,y_size/2+20,20};
    Vector3 Point2 = {x_size/2+20,y_size/2+20,-20};
    Vector3 Point3 = {x_size/2+20,y_size/2-20,20};
    Vector3 Point4 = {x_size/2-20,y_size/2+20,20};
    Vector3 Point5 = {x_size/2-20,y_size/2-20,20};
    Vector3 Point6 = {x_size/2+20,y_size/2-20,-20};
    Vector3 Point7 = {x_size/2-20,y_size/2+20,-20};
    Vector3 Point8 = {x_size/2-20,y_size/2-20,-20};
    Cube cube = {Point1, Point2, Point3, Point4, Point5, Point6, Point7, Point8};

    //원근 투영 적용
    Point2D new_point[8];
    for (int i = 0; i < 8; i++) {
        new_point[i] = Vec3ToCamera(&cube.point[i]);
    }

    // 정육면체의 12개 모서리를 연결하는 코드
    Line2D cube_lines[12];
    Line2D prev_cube_lines[12];

    // 모든 라인 스택 초기화
    for (int i = 0; i < 12; i++) {
        initStack(&cube_lines[i].line);
        initStack(&prev_cube_lines[i].line);
    }

    // 정육면체 모서리 연결 (12개의 모서리)
    // 앞면 (z = 20) 4개 모서리
    DrawLine(&new_point[0], &new_point[2], &cube_lines[0], &prev_cube_lines[0]);  // Point1 - Point3
    DrawLine(&new_point[2], &new_point[4], &cube_lines[1], &prev_cube_lines[1]);  // Point3 - Point5
    DrawLine(&new_point[4], &new_point[3], &cube_lines[2], &prev_cube_lines[2]);  // Point5 - Point4
    DrawLine(&new_point[3], &new_point[0], &cube_lines[3], &prev_cube_lines[3]);  // Point4 - Point1

    // 뒷면 (z = -20) 4개 모서리
    DrawLine(&new_point[1], &new_point[5], &cube_lines[4], &prev_cube_lines[4]);  // Point2 - Point6
    DrawLine(&new_point[5], &new_point[7], &cube_lines[5], &prev_cube_lines[5]);  // Point6 - Point8
    DrawLine(&new_point[7], &new_point[6], &cube_lines[6], &prev_cube_lines[6]);  // Point8 - Point7
    DrawLine(&new_point[6], &new_point[1], &cube_lines[7], &prev_cube_lines[7]);  // Point7 - Point2

    // 앞면과 뒷면을 연결하는 4개 모서리
    DrawLine(&new_point[0], &new_point[1], &cube_lines[8], &prev_cube_lines[8]);   // Point1 - Point2
    DrawLine(&new_point[2], &new_point[5], &cube_lines[9], &prev_cube_lines[9]);   // Point3 - Point6
    DrawLine(&new_point[4], &new_point[7], &cube_lines[10], &prev_cube_lines[10]); // Point5 - Point8
    DrawLine(&new_point[3], &new_point[6], &cube_lines[11], &prev_cube_lines[11]); // Point4 - Point7

    // 모든 라인 업데이트
    for (int i = 0; i < 12; i++) {
        update(&cube_lines[i].line, &prev_cube_lines[i].line);
    }

    char key;
    while (1) {
        if (kbhit()) {
            key = getchar();
            if (key == 'p') {
                break;  // 'q' 입력 시 종료
            } else if (key == 'w') {
                moveCursor(0,  -1);
                MoveCube(&cube, 'y', 1, cube_lines, prev_cube_lines);
            } else if (key == 'a') {
                moveCursor(0,  -1);
                printf("A 키 눌림\n");
                MoveCube(&cube, 'x', -1, cube_lines, prev_cube_lines);
            } else if (key == 's') {
                moveCursor(0,  -1);
                printf("S 키 눌림\n");
                MoveCube(&cube, 'y', -1, cube_lines, prev_cube_lines);
            } else if (key == 'd') {
                moveCursor(0,  -1);
                printf("D 키 눌림\n");
                MoveCube(&cube, 'x', 1, cube_lines, prev_cube_lines);
            } else if (key == 'q') {
                moveCursor(0,  -1);
                printf("Q 키 눌림\n");
                MoveCube(&cube, 'z', 1, cube_lines, prev_cube_lines);
            } else if (key == 'e') {
                moveCursor(0,  -1);
                printf("E 키 눌림\n");
                MoveCube(&cube, 'z', -1, cube_lines, prev_cube_lines);
            }

        }
        usleep(1000); // CPU 사용률 낮추기 위해 딜레이
    }
    return 0;
}