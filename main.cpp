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

//카메라의 좌표
#define CAMERA_X (x_size / 2)
#define CAMERA_Y (y_size / 2)
#define CAMERA_Z -70

// 아웃코드 정의, 정해진 화면 밖으로 넘어가면 선 렌더링 안하는거임
#define INSIDE 0  // 0000
#define LEFT   1  // 0001
#define RIGHT  2  // 0010
#define BOTTOM 4  // 0100
#define TOP    8  // 1000

// 황금비 정의
#define PHI 1.618033988749895f  // (1 + sqrt(5)) / 2

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
//정20면체 구조체 (12개의 꼭짓점)
struct Icosahedron {
    Vector3 point[12];
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
void MoveIcosahedron(struct Icosahedron* icosa, char axis, int length, Line2D* icosa_lines, Line2D* prev_icosa_lines);
void RotateIcosahedron(struct Icosahedron* icosa, char axis, float angle, Line2D* icosa_lines, Line2D* prev_icosa_lines);
void InitIcosahedron(struct Icosahedron* icosa, float scale);

//화면 모든 픽셀값에 대한 정보
char screen[y_size][x_size] = {};

//키 입력 체크
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
void initScreen() {
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

//업데이트
void update(Stack* obj, Stack* prev_obj) {
    int obj_size = getSize(obj);
    int prev_size = getSize(prev_obj);
    moveCursor(0, -1);

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

// DrawLine 함수
void DrawLine(Point2D *p1, Point2D *p2, struct Line2D *line, struct Line2D *prev_line) {
    Line2D duplicate_line = *line;
    initStack(&prev_line->line);
    *prev_line = duplicate_line;

    initStack(&line->line);

    // 클리핑된 점들
    Point2D clipped_p1, clipped_p2;

    // 선분 클리핑 수행
    if (!clipLine(p1, p2, &clipped_p1, &clipped_p2)) {
        return;
    }

    // 클리핑된 점들을 반올림
    int start_x = round(clipped_p1.x);
    int start_y = round(clipped_p1.y);
    int end_x = round(clipped_p2.x);
    int end_y = round(clipped_p2.y);

    if (start_x >= x_size-1) start_x = x_size-2;
    if (end_x >= x_size-1) end_x = x_size-2;
    if (start_x <= 0) start_x = 1;
    if (end_x <= 0) end_x = 1;
    if (start_y >= y_size-2) start_y = y_size-3;
    if (end_y >= y_size-2) end_y = y_size-3;

    // 브레젠험 알고리즘
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

    // 마지막 점도 추가
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
    PointInCamera.y = (y / z) * d;

    // 종횡비 보정
    float aspect_ratio = (float)y_size / x_size;
    PointInCamera.x *= aspect_ratio;

    // 화면 좌표로 변환
    PointInCamera.x = PointInCamera.x * (x_size/2) + (x_size/2);
    PointInCamera.y = PointInCamera.y * (y_size/2) + (y_size/2);

    return PointInCamera;
}

// 정20면체 초기화 함수
void InitIcosahedron(struct Icosahedron* icosa, float scale) {
    // 정20면체의 12개 꼭짓점 좌표 (황금비 사용)
    // 3개의 직교하는 황금직사각형의 꼭짓점들

    // 첫 번째 황금직사각형 (YZ 평면)
    icosa->point[0] = (Vector3){0, scale, scale * PHI};
    icosa->point[1] = (Vector3){0, -scale, scale * PHI};
    icosa->point[2] = (Vector3){0, scale, -scale * PHI};
    icosa->point[3] = (Vector3){0, -scale, -scale * PHI};

    // 두 번째 황금직사각형 (XZ 평면)
    icosa->point[4] = (Vector3){scale, scale * PHI, 0};
    icosa->point[5] = (Vector3){-scale, scale * PHI, 0};
    icosa->point[6] = (Vector3){scale, -scale * PHI, 0};
    icosa->point[7] = (Vector3){-scale, -scale * PHI, 0};

    // 세 번째 황금직사각형 (XY 평면)
    icosa->point[8] = (Vector3){scale * PHI, 0, scale};
    icosa->point[9] = (Vector3){scale * PHI, 0, -scale};
    icosa->point[10] = (Vector3){-scale * PHI, 0, scale};
    icosa->point[11] = (Vector3){-scale * PHI, 0, -scale};

    // 화면 중심으로 이동
    for (int i = 0; i < 12; i++) {
        icosa->point[i].x += x_size/2;
        icosa->point[i].y += y_size/2;
        icosa->point[i].z += 0;
    }
}

//정20면체 이동 함수
void MoveIcosahedron(struct Icosahedron* icosa, char axis, int length, Line2D* icosa_lines, Line2D* prev_icosa_lines) {
    // 먼저 이전 정20면체를 완전히 지우기
    for (int i = 0; i < 30; i++) {  // 정20면체는 30개의 모서리
        DelObj(&icosa_lines[i].line);
    }

    // 정20면체의 모든 점 이동
    for (int i = 0; i < 12; i++) {
        if (axis == 'x') {
            icosa->point[i].x += length;
        }
        else if (axis == 'y') {
            icosa->point[i].y += length;
        }
        else if (axis == 'z') {
            icosa->point[i].z += length;
        }
    }

    // 새로운 2D 좌표로 변환
    Point2D new_point[12];
    for (int i = 0; i < 12; i++) {
        new_point[i] = Vec3ToCamera(&icosa->point[i]);
    }

    // 모든 라인 스택 초기화
    for (int i = 0; i < 30; i++) {
        initStack(&icosa_lines[i].line);
        initStack(&prev_icosa_lines[i].line);
    }

    // 정20면체의 30개 모서리 연결
    int edges[30][2] = {
        {0, 1}, {0, 4}, {0, 5}, {0, 8}, {0, 10},
        {1, 6}, {1, 7}, {1, 8}, {1, 10}, {2, 3},
        {2, 4}, {2, 5}, {2, 9}, {2, 11}, {3, 6},
        {3, 7}, {3, 9}, {3, 11}, {4, 5}, {4, 8},
        {4, 9}, {5, 10}, {5, 11}, {6, 7}, {6, 8},
        {6, 9}, {7, 10}, {7, 11}, {8, 9}, {10, 11}
    };

    for (int i = 0; i < 30; i++) {
        DrawLine(&new_point[edges[i][0]], &new_point[edges[i][1]], &icosa_lines[i], &prev_icosa_lines[i]);
    }

    // 모든 라인 업데이트 (새로운 정20면체 그리기)
    for (int i = 0; i < 30; i++) {
        int obj_size = getSize(&icosa_lines[i].line);
        for (int j = 0; j < obj_size; j++) {
            Point2D* pointPtr = (Point2D*)getAtIndex(&icosa_lines[i].line, j);
            if (pointPtr != NULL) {
                Point2D point = *pointPtr;
                moveCursor(round(point.x)*2, round(point.y));
                printf("* ");
            }
        }
    }
}

//정20면체 회전 함수
void RotateIcosahedron(struct Icosahedron* icosa, char axis, float angle, Line2D* icosa_lines, Line2D* prev_icosa_lines) {
    // 먼저 이전 정20면체를 완전히 지우기
    for (int i = 0; i < 30; i++) {
        DelObj(&icosa_lines[i].line);
    }

    angle = (angle * M_PI / 180.0);

    // 정20면체의 중심점 계산
    Vector3 center = {0, 0, 0};
    for (int i = 0; i < 12; i++) {
        center.x += icosa->point[i].x;
        center.y += icosa->point[i].y;
        center.z += icosa->point[i].z;
    }
    center.x /= 12.0;
    center.y /= 12.0;
    center.z /= 12.0;

    // 정20면체의 모든 점을 중심점 기준으로 회전
    for (int i = 0; i < 12; i++) {
        // 1. 중심점으로 이동
        float x = icosa->point[i].x - center.x;
        float y = icosa->point[i].y - center.y;
        float z = icosa->point[i].z - center.z;

        // 2. 회전 변환 적용
        float new_x, new_y, new_z;
        if (axis == 'x') {
            new_x = x;
            new_y = cos(angle) * y - sin(angle) * z;
            new_z = sin(angle) * y + cos(angle) * z;
        }
        else if (axis == 'y') {
            new_x = cos(angle) * x + sin(angle) * z;
            new_y = y;
            new_z = -sin(angle) * x + cos(angle) * z;
        }
        else if (axis == 'z') {
            new_x = cos(angle) * x - sin(angle) * y;
            new_y = sin(angle) * x + cos(angle) * y;
            new_z = z;
        }

        // 3. 다시 원래 위치로 이동
        icosa->point[i].x = new_x + center.x;
        icosa->point[i].y = new_y + center.y;
        icosa->point[i].z = new_z + center.z;
    }

    // 새로운 2D 좌표로 변환
    Point2D new_point[12];
    for (int i = 0; i < 12; i++) {
        new_point[i] = Vec3ToCamera(&icosa->point[i]);
    }

    // 모든 라인 스택 초기화
    for (int i = 0; i < 30; i++) {
        initStack(&icosa_lines[i].line);
        initStack(&prev_icosa_lines[i].line);
    }

    // 정20면체의 30개 모서리 연결
    int edges[30][2] = {
        {0, 1}, {0, 4}, {0, 5}, {0, 8}, {0, 10},
        {1, 6}, {1, 7}, {1, 8}, {1, 10}, {2, 3},
        {2, 4}, {2, 5}, {2, 9}, {2, 11}, {3, 6},
        {3, 7}, {3, 9}, {3, 11}, {4, 5}, {4, 8},
        {4, 9}, {5, 10}, {5, 11}, {6, 7}, {6, 8},
        {6, 9}, {7, 10}, {7, 11}, {8, 9}, {10, 11}
    };

    for (int i = 0; i < 30; i++) {
        DrawLine(&new_point[edges[i][0]], &new_point[edges[i][1]], &icosa_lines[i], &prev_icosa_lines[i]);
    }

    // 모든 라인 업데이트 (새로운 정20면체 그리기)
    for (int i = 0; i < 30; i++) {
        int obj_size = getSize(&icosa_lines[i].line);
        for (int j = 0; j < obj_size; j++) {
            Point2D* pointPtr = (Point2D*)getAtIndex(&icosa_lines[i].line, j);
            if (pointPtr != NULL) {
                Point2D point = *pointPtr;
                moveCursor(round(point.x)*2, round(point.y));
                printf("* ");
            }
        }
    }
}

//main 함수
int main() {
    setbuf(stdout, NULL);
    //콘솔 청소 + 화면 배열 초기화
    clearConsole();
    initScreen();
    ShowScreen();

    //정20면체 생성 및 초기화
    struct Icosahedron icosahedron;
    InitIcosahedron(&icosahedron, 20.0f);  // 크기 20으로 초기화

    //원근 투영 적용
    Point2D new_point[12];
    for (int i = 0; i < 12; i++) {
        new_point[i] = Vec3ToCamera(&icosahedron.point[i]);
    }

    // 정20면체의 30개 모서리를 연결하는 코드
    Line2D icosa_lines[30];
    Line2D prev_icosa_lines[30];

    // 모든 라인 스택 초기화
    for (int i = 0; i < 30; i++) {
        initStack(&icosa_lines[i].line);
        initStack(&prev_icosa_lines[i].line);
    }

    // 정20면체의 30개 모서리 연결
    int edges[30][2] = {
        {0, 1}, {0, 4}, {0, 5}, {0, 8}, {0, 10},
        {1, 6}, {1, 7}, {1, 8}, {1, 10}, {2, 3},
        {2, 4}, {2, 5}, {2, 9}, {2, 11}, {3, 6},
        {3, 7}, {3, 9}, {3, 11}, {4, 5}, {4, 8},
        {4, 9}, {5, 10}, {5, 11}, {6, 7}, {6, 8},
        {6, 9}, {7, 10}, {7, 11}, {8, 9}, {10, 11}
    };

    for (int i = 0; i < 30; i++) {
        DrawLine(&new_point[edges[i][0]], &new_point[edges[i][1]], &icosa_lines[i], &prev_icosa_lines[i]);
    }

    // 모든 라인 업데이트
    for (int i = 0; i < 30; i++) {
        update(&icosa_lines[i].line, &prev_icosa_lines[i].line);
    }

    // 자동 회전 (주석 처리됨)
    // for (int i = 0; i < 1000; i++) {
    //     RotateIcosahedron(&icosahedron, 'x', +0.5, icosa_lines, prev_icosa_lines);
    //     RotateIcosahedron(&icosahedron, 'y', +0.8, icosa_lines, prev_icosa_lines);
    //     RotateIcosahedron(&icosahedron, 'z', +1.2, icosa_lines, prev_icosa_lines);
    //     usleep(20000);
    // }

    char key;
    while (1) {
        if (kbhit()) {
            key = getchar();
            if (key == 'p') {
                break;  // 'p' 입력 시 종료
            } else if (key == 'w') {
                moveCursor(0, -1);
                MoveIcosahedron(&icosahedron, 'y', 1, icosa_lines, prev_icosa_lines);
            } else if (key == 'a') {
                moveCursor(0, -1);
                printf("A 키 눌림\n");
                MoveIcosahedron(&icosahedron, 'x', -1, icosa_lines, prev_icosa_lines);
            } else if (key == 's') {
                moveCursor(0, -1);
                printf("S 키 눌림\n");
                MoveIcosahedron(&icosahedron, 'y', -1, icosa_lines, prev_icosa_lines);
            } else if (key == 'd') {
                moveCursor(0, -1);
                printf("D 키 눌림\n");
                MoveIcosahedron(&icosahedron, 'x', 1, icosa_lines, prev_icosa_lines);
            } else if (key == 'q') {
                moveCursor(0, -1);
                printf("Q 키 눌림\n");
                MoveIcosahedron(&icosahedron, 'z', 1, icosa_lines, prev_icosa_lines);
            } else if (key == 'e') {
                moveCursor(0, -1);
                printf("E 키 눌림\n");
                MoveIcosahedron(&icosahedron, 'z', -1, icosa_lines, prev_icosa_lines);
            } else if (key == 'j') {
                moveCursor(0, -1);
                printf("J 키 눌림\n");
                RotateIcosahedron(&icosahedron, 'z', -2, icosa_lines, prev_icosa_lines);
            } else if (key == 'l') {
                moveCursor(0, -1);
                printf("L 키 눌림\n");
                RotateIcosahedron(&icosahedron, 'z', +2, icosa_lines, prev_icosa_lines);
            } else if (key == 'i') {
                moveCursor(0, -1);
                printf("I 키 눌림\n");
                RotateIcosahedron(&icosahedron, 'x', -2, icosa_lines, prev_icosa_lines);
            } else if (key == 'k') {
                moveCursor(0, -1);
                printf("K 키 눌림\n");
                RotateIcosahedron(&icosahedron, 'x', +2, icosa_lines, prev_icosa_lines);
            } else if (key == 'u') {
                moveCursor(0, -1);
                printf("U 키 눌림\n");
                RotateIcosahedron(&icosahedron, 'y', -2, icosa_lines, prev_icosa_lines);
            } else if (key == 'o') {
                moveCursor(0, -1);
                printf("O 키 눌림\n");
                RotateIcosahedron(&icosahedron, 'y', +2, icosa_lines, prev_icosa_lines);
            }

        }
        usleep(1000); // CPU 사용률 낮추기 위해 딜레이
    }
    return 0;
}