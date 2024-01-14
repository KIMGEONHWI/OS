#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define MAX_FUTURE_ACCESSES 5000    // 최대 미래 페이지 액세스 수


// 페이지 교체 알고리즘 종류
typedef enum {
    OPTIMAL,
    FIFO,
    LRU,
    SECOND_CHANCE
} Algorithm;

// 가상주소 처리를 위한 페이지 테이블 엔트리
typedef struct {
    int valid;         // 유효한 페이지인지 여부
    int frame;         // 물리 메모리의 프레임 번호
    int reference_bit; // Second-Chance 알고리즘을 위한 참조 비트
    int last_used;     // LRU 알고리즘을 위한 마지막 사용 시간
} PageTableEntry;

// 페이지 테이블
PageTableEntry* page_table;

int page_size; // 페이지 크기를 저장하는 전역 변수

int physical_memory_size;   // 물리 메모리 크기

int num_frames;

unsigned long max_virtual_address;


// 물리 메모리
int* physical_memory;

int front = 0;           // 큐의 프론트 인덱스

// Optimal 알고리즘에 따라 페이지를 교체하는 함수
void replace_page_optimal(int virtual_address, int* page_faults, int current_access_index, int* future_accesses) {
    int longest_unused = -1;
    int frame_to_replace = -1;

    for (int i = 0; i < num_frames; i++) {
        int found = 0;
        int future_use_distance = 0;

        for (int j = current_access_index; j < MAX_FUTURE_ACCESSES; j++) {
            if (future_accesses[j] / page_size == physical_memory[i] / page_size) {
                found = 1;
                future_use_distance = j - current_access_index;
                break;
            }
        }

        if (!found) { // 페이지가 미래에 사용되지 않을 경우
            frame_to_replace = i;
            break;
        } else if (future_use_distance > longest_unused) { // 가장 늦게 사용될 페이지 찾기
            longest_unused = future_use_distance;
            frame_to_replace = i;
        }
    }

    if (frame_to_replace != -1) {
        int page_number = virtual_address / page_size;
        int replaced_page = physical_memory[frame_to_replace] / page_size;
        page_table[replaced_page].valid = 0; // 이전 페이지 무효화

        page_table[page_number].frame = frame_to_replace;
        page_table[page_number].valid = 1;
        physical_memory[frame_to_replace] = virtual_address; // 물리 메모리 업데이트
        (*page_faults)++;
    }
}

// FIFO 페이지 교체 알고리즘
void replace_page_fifo(int virtual_address, int* page_faults, PageTableEntry* page_table, int* physical_memory) {

    int frame_to_replace;

    // 페이지 번호 계산
    int page_number = virtual_address / page_size;

    if (front < num_frames) {
        // 아직 모든 프레임이 할당되지 않았으면 새 프레임 할당
        frame_to_replace = front;
        front++;
    }
    else {
        front = 0;
        frame_to_replace = front;
        front++;
    }

    int replaced_page = physical_memory[frame_to_replace] / page_size;
    page_table[replaced_page].valid = 0;

    // 페이지 테이블 및 물리 메모리 업데이트
    page_table[page_number].frame = frame_to_replace;
    physical_memory[frame_to_replace] = page_number; // 프레임에 페이지 번호를 저장
    (*page_faults)++;
}

// LRU 알고리즘에 따라 페이지를 교체하는 함수
void replace_page_lru(int virtual_address, int* page_faults, PageTableEntry* page_table, int* physical_memory, int num_frames, int current_time) {
    int lruFrame = -1;
    int minTime = INT_MAX;

    // LRU 프레임 찾기
    for (int i = 0; i < num_frames; ++i) {
        int page_index = physical_memory[i] / page_size;
        if (page_table[page_index].valid && page_table[page_index].last_used <= minTime) {
            minTime = page_table[page_index].last_used;
            lruFrame = i;
        }
    }

    // 페이지 교체 수행
    if (lruFrame != -1) {
        int page_number = virtual_address / page_size;
        int replaced_page = physical_memory[lruFrame] / page_size;
        page_table[replaced_page].valid = 0;  // 이전 페이지 무효화

        page_table[page_number].frame = lruFrame;
        page_table[page_number].last_used = current_time; // 새 페이지의 last_used 업데이트
        physical_memory[lruFrame] = page_number * page_size; // 물리 메모리 업데이트
        (*page_faults)++;
    }
}

// Second-Chance 알고리즘에 따라 페이지를 교체하는 함수
void replace_page_second_chance(int virtual_address, int* page_faults) {
    while (1) {
        int page_index = physical_memory[front] / page_size;

        if (page_table[page_index].reference_bit == 1) {
            // 참조 비트가 설정된 경우: 참조 비트를 지우고 큐의 뒤로 이동
            page_table[page_index].reference_bit = 0;
            front = (front + 1) % num_frames;
        } else {
            // 참조 비트가 지워진 페이지를 교체 대상으로 선택
            int replaced_page = physical_memory[front] / page_size;
            page_table[replaced_page].valid = 0; // 이전 페이지 무효화

            int page_number = virtual_address / page_size;
            page_table[page_number].frame = front;
            page_table[page_number].valid = 1;
            physical_memory[front] = virtual_address; // 물리 메모리 업데이트

            front = (front + 1) % num_frames; // 큐의 다음 위치로 이동
            (*page_faults)++;
            break;
        }
    }
}

int handle_virtual_address(int virtual_address, int* page_faults, int* current_frame, Algorithm algorithm, char* page_fault_occurred, int current_access_index, int* future_accesses, int* current_time) {
    int page_number = virtual_address / page_size;
    int frame_number;

    // 페이지 테이블에서 해당 가상 주소의 페이지 번호에 해당하는 엔트리 찾기
    PageTableEntry* page_entry = &page_table[page_number];

    if (page_entry->valid) {
        // 페이지가 유효하고, 이미 메모리에 로드되어 있는 경우
        frame_number = page_entry->frame;
        *page_fault_occurred = 'H'; // 페이지 히트

        // LRU 알고리즘의 경우 last_used 업데이트
        if (algorithm == LRU) {
            page_table[page_number].last_used = *current_time;
        }

        // Second-Chance 알고리즘의 경우 reference_bit 업데이트
        if (algorithm == SECOND_CHANCE) {
            page_table[page_number].reference_bit = 1;
        }
    }
    else {
        // 페이지 부재(Page Fault)가 발생한 경우
        *page_fault_occurred = 'F'; // 페이지 폴트
        (*page_faults)++;
        page_entry->valid = 1; // 페이지 엔트리를 유효하게 설정

        if (*current_frame < num_frames) {
            // 물리 메모리에 여유가 있는 경우
            frame_number = *current_frame;
            // Second-Chance 알고리즘의 경우 reference_bit 설정
            if (algorithm == SECOND_CHANCE) {
                page_table[page_number].reference_bit = 1;
            }
        }
        else {
            // 물리 메모리에 여유가 없어 페이지 교체가 필요한 경우
            switch (algorithm) {
            case OPTIMAL:
                replace_page_optimal(virtual_address, page_faults, current_access_index, future_accesses);
                break;
            case FIFO:
                replace_page_fifo(virtual_address, page_faults, page_table, physical_memory);
                break;
            case LRU:
                replace_page_lru(virtual_address, page_faults, page_table, physical_memory, num_frames, *current_time);
                break;
            case SECOND_CHANCE:
                replace_page_second_chance(virtual_address, page_faults);
                break;
            default:
                fprintf(stderr, "알 수 없는 페이지 교체 알고리즘입니다.\n");
                exit(1);
            }
            frame_number = page_entry->frame; // 교체 후에 새로운 페이지의 프레임 번호를 가져옴
        }

        page_table[page_number].frame = frame_number;
        physical_memory[frame_number] = virtual_address;
        (*current_frame)++;
    }

    // 페이지 액세스마다 시간 증가
    (*current_time)++;

    return *page_faults;
}

// 페이지 교체 결과를 기록하는 함수
void record_page_replacement_result(FILE* output_file, int virtual_address, int page_faults, char page_fault, int count, int page_fault_count) {
    int page_number = virtual_address / page_size; // 전역 변수 page_size 사용
    int frame_number = page_table[page_number].frame;
    int offset = virtual_address % page_size; // 전역 변수 page_size 사용
    int physical_address = frame_number * page_size + offset; // 전역 변수 page_size 사용

    fprintf(output_file, "| %4d | %7d | %10d | %9d | %7d | %12c |\n",
        count, virtual_address, page_number, frame_number, physical_address, page_fault);

    // 마지막 행에서 페이지 폴트의 총 개수를 출력
    if (count == 5000) {
        fprintf(output_file, "==================================================================\n");
        fprintf(output_file, "Total Number of Page Faults: %d\n", page_fault_count);
    }
}

int main() {
    int virtual_address_length; // 가상주소의 길이
    int algorithm_choice;       // 페이지 교체 알고리즘 선택
    int input_choice;           // 가상주소 스트링 입력 방식 선택
    char input_filename[100];   // 입력 파일 이름

    // 가상주소의 길이 선택
    while (1) {
        printf("A. Simulation에 사용할 가상주소 길이를 선택하시오 (1. 18bits     2. 19bits     3. 20bits): ");
        scanf("%d", &virtual_address_length);

        if (virtual_address_length == 1) {
            virtual_address_length = 18;
            break;
        }
        else if (virtual_address_length == 2) {
            virtual_address_length = 19;
            break;
        }
        else if (virtual_address_length == 3) {
            virtual_address_length = 20;
            break;
        }
        else {
            printf("잘못된 입력입니다. 1, 2, 3 중 하나를 입력해야 합니다.\n");
        }
    }

    // 가상 메모리 크기 계산 및 설정
    unsigned long max_virtual_address;
    if (virtual_address_length == 18) {
        max_virtual_address = 256 * 1024; // 256KB
    }
    else if (virtual_address_length == 19) {
        max_virtual_address = 512 * 1024; // 512KB
    }
    else if (virtual_address_length == 20) {
        max_virtual_address = 1024 * 1024; // 1MB
    }

    // 페이지 크기 선택 및 설정
    printf("B. Simulation에 사용할 페이지(프레임)의 크기를 선택하시오 (1. 1KB    2. 2KB     3. 4KB): ");
    scanf("%d", &page_size);
    if (page_size == 1) {
        page_size = 1024; // 1KB
    }
    else if (page_size == 2) {
        page_size = 2048; // 2KB
    }
    else if (page_size == 3) {
        page_size = 4096; // 4KB
    }
    else {
        printf("잘못된 입력입니다. 페이지 크기는 1, 2, 3 중 하나를 입력해야 합니다.\n");
        return 0;
    }

    // 물리 메모리 크기 선택
    printf("C. Simulation에 사용할 물리 메모리의 크기를 선택하시오 (1. 32KB     2. 64KB): ");
    scanf("%d", &physical_memory_size);
    if (physical_memory_size == 1) {
        physical_memory_size = 32 * 1024; // 32KB
    }
    else if (physical_memory_size == 2) {
        physical_memory_size = 64 * 1024; // 64KB
    }
    else {
        printf("잘못된 입력입니다. 물리 메모리 크기는 1 또는 2를 입력해야 합니다.\n");
        return 0;
    }

    // 페이지 교체 알고리즘 선택
    printf("D. Simulation에 적용할 Page Replacement 알고리즘을 선택하시오 (1. Optimal     2. FIFO     3. LRU    4. Second-Chance): ");
    scanf("%d", &algorithm_choice);
    if (algorithm_choice < 1 || algorithm_choice > 4) {
        printf("잘못된 입력입니다. 페이지 교체 알고리즘은 1에서 4 사이의 값을 입력해야 합니다.\n");
        return 0;
    }
    Algorithm selected_algorithm = (Algorithm)(algorithm_choice - 1); // enum으로 변환

    // 가상주소 스트링 입력 방식 선택
    printf("E. 가상주소 스트링 입력방식을 선택하시오 (1. input.in 자동 생성 2. 기존 파일 사용): ");
    scanf("%d", &input_choice);
    if (input_choice != 1 && input_choice != 2) {
        printf("잘못된 입력입니다. 가상주소 스트링 입력 방식은 1 또는 2 중에서 선택해야 합니다.\n");
        return 0;
    }
    // 가상주소 입력 파일 생성 또는 기존 파일 사용
    FILE* input_file;

    if (input_choice == 1) {
        // input.in 파일 자동 생성
        input_file = fopen("input.in", "w");
        srand(time(NULL)); // 난수 생성 초기화
        int max_virtual_address = (1 << virtual_address_length) - 1;
        for (int i = 0; i < 5000; i++) {
            int virtual_address = rand() % max_virtual_address;
            fprintf(input_file, "%d\n", virtual_address);
        }
        fclose(input_file);
        input_file = fopen("input.in", "r"); // input.in 파일을 읽기 모드로 다시 열기
    }
    else if (input_choice == 2) {
        // 기존 파일 사용
        printf("F. 입력 파일 이름을 입력하시오: ");
        scanf("%s", input_filename);
        input_file = fopen(input_filename, "r");
        if (input_file == NULL) {
            printf("입력 파일을 열 수 없습니다. 파일 이름을 확인해주세요.\n");
            return 1;
        }
    }
    else {
        printf("잘못된 입력입니다. 1 또는 2를 입력해야 합니다.\n");
        return 0;
    }

    // 프레임 개수 계산
    num_frames = physical_memory_size / page_size;

    // 페이지 테이블 및 물리 메모리 초기화
    int total_pages = max_virtual_address / page_size; // 전체 페이지 수 계산
    page_table = malloc(total_pages * sizeof(PageTableEntry));
    physical_memory = malloc(physical_memory_size * sizeof(int));


    for (int i = 0; i < total_pages; i++) {
        page_table[i].valid = 0;
        page_table[i].frame = -1;
        page_table[i].reference_bit = 0;
    }
    for (int i = 0; i < physical_memory_size; i++) {
        physical_memory[i] = -1; // 물리 메모리를 초기값(-1)으로 설정
    }

    FILE* output_file;
    char output_filename[100];

    // 페이지 교체 알고리즘에 따라 출력 파일 이름 설정
    switch (selected_algorithm) {
    case OPTIMAL:
        sprintf(output_filename, "output.opt");
        break;
    case FIFO:
        sprintf(output_filename, "output.fifo");
        break;
    case LRU:
        sprintf(output_filename, "output.lru");
        break;
    case SECOND_CHANCE:
        sprintf(output_filename, "output.sc");
        break;
    default:
        fprintf(stderr, "알 수 없는 페이지 교체 알고리즘입니다.\n");
        return 1;
    }

    // 출력 파일 열기
    output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        fprintf(stderr, "출력 파일을 생성할 수 없습니다.\n");
        return 1;
    }

    // 한글 헤더 추가
    fprintf(output_file, "     NO.      V.A      Page No.   Frame No.      P.A.     Page Fault \n");

    int virtual_address, page_faults = 0, current_frame = 0, current_time = 0;
    char page_fault_occurred = 'F';

    // 순서 추적을 위한 카운터
    int count = 0;
    int page_fault_count = 0;

    // 가상 주소 처리 루프
    int future_accesses[MAX_FUTURE_ACCESSES];
    int future_index = 0;

    // 미래 액세스 정보를 future_accesses 배열에 저장
    while (fscanf(input_file, "%d", &future_accesses[future_index]) != EOF && future_index < MAX_FUTURE_ACCESSES) {
        future_index++;
    }

    rewind(input_file);  // 입력 파일 포인터를 다시 시작 부분으로 이동

    future_index = 0; // 현재 처리 중인 가상 주소 인덱스 초기화

    while (fscanf(input_file, "%d", &virtual_address) != EOF) {
        page_faults = handle_virtual_address(virtual_address, &page_faults, &current_frame, selected_algorithm, &page_fault_occurred, future_index, future_accesses, &current_time);

        if (page_fault_occurred == 'F') {
            page_fault_count++;
        }

        // 결과 기록
        record_page_replacement_result(output_file, virtual_address, page_faults, page_fault_occurred, ++count, page_fault_count);

        future_index++; // 현재 처리 중인 가상 주소 인덱스 증가
    }

    // 파일 및 메모리 자원 정리
    fclose(input_file);
    fclose(output_file);
    if (page_table) free(page_table);
    if (physical_memory) free(physical_memory);

    return 0;
}

