#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/vaddr.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
/* f는 어셈블리가 값을 저장한 스택 영역의 주소이다.
   syscall_entry가 CPU의 저장 정보를 커널 스택에 쌓고, 
   그 스택 영역을 intr_frame 구조체로 해석해서 syscall_handler()가 사용 */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call!\n");
	thread_exit ();
}

/* 사용자 메모리 접근을 검사하는 헬퍼 함수 */
/* 1. 사용자 주소인지 확인: 커널 영역 주소를 차단 */
/* 2. 실제로 접근 가능한지(매핑됐는지) 확인: 매핑되지 않은 주소 등의 접근을 방지 */
/* 3. 읽기, 쓰기 범위 확인: 여러 바이트에 걸친 접근에서 잘못된 영역을 놓치지 않도록 함 */
/* 4. 접근 권한 확인 */

/* 시스템 콜 함수별로 검증 함수 제작 */
/* 포인터 검사가 필요한 함수 목록 */
/* fork / exec, create, remove, open /read, write / mmap, munmap / chdir, makdir / symlink / mount, umount  */

/* 문자열 전체 검증 함수 
   1. 포인터가 NULL인지 확인한다.
   2. 현재 주소가 사용자 주소 영역인지 확인한다.
   3. 현재 주소의 페이지가 매핑되어 있는지 확인한다.
   4. 검증한 주소에서 문자를 읽는다.
   5. '\0'이면 종료하고, 아니라면 다음 문자로 이동해 반복한다.
   6. 페이지 경계를 넘으면 다음 페이지도 검사한다.*/
bool
validate_user_string(const char *ptr) {
	if (ptr == NULL)
		return false;

	struct thread *t = thread_current();
	
	while (true) {					
		if (!is_user_vaddr(ptr))		// 사용자 주소인지 확인. ptr: 문자열이 저장된 메모리 주소, *ptr: 그 주소에 저장된 문자 하나
			return false;
		
		if (!pml4_get_page(t->pml4, ptr))	// 주소가 페이지에 매핑되어있는지 확인
			return false;
		
		if (*ptr == '/0')			// 널 문자면 문자열의 끝
			return true;
	
		ptr++;						// 다음 문자로 변환
	}
}

/* 메모리 범위 검사 
   1. 포인터와 크기가 유효한지 확인한다.
   2. 버퍼가 사용하는 주소 범위가 사용자 영역인지 확인한다.
   3. 범위에 포함된 모든 페이지가 매핑되어 있는지 확인한다.
   4. 커널이 버퍼에 쓸 경우 쓰기 권한도 확인한다.
   5. 주소 계산의 오버플로도 고려한다. */
bool 
valiadate_user_buffer(void * buffer, size_t size) {
	if (buffer == NULL)
		return false;
	
	if (size == 0)
		return false;

	uint8_t *ptr = (uint8_t) buffer;
	uint8_t *end = ptr + size - 1;
	
	/* pg_round_down(ptr);		 현재 포인터가 속한 페이지의 시작 주소
	   pg_round_down(end);		 버퍼의 끝이 속한 페이지의 시작 주소
	   pg_round_up(end);   		 버퍼의 끝이 속한 페이지의 다음 주소 */
bool
validate_user_buffer (void *buffer, size_t size)
{
    if (buffer == NULL)
        return false;

    if (size == 0)
        return true;

    uint64_t start = (uint64_t) buffer;

    // 주소 덧셈 오버플로 방지
    if (size - 1 > UINT64_MAX - start)
        return false;

    uint64_t last = start + size - 1;

    // 사용자 주소 범위인지 검사
    if (!is_user_vaddr ((void *) start) ||
        !is_user_vaddr ((void *) last))
        return false;

    uint8_t *ptr = (uint8_t *) pg_round_down (buffer);
    uint8_t *end = (uint8_t *) pg_round_down ((void *) last);

    for (; ptr <= end; ptr += PGSIZE)
    {
        if (!is_user_vaddr (ptr))
            return false;

        if (pml4_get_page (thread_current ()->pml4, ptr) == NULL)
            return false;
    }

    return true;
}

/* 단일 주소 검사
   1. 포인터가 유효한지
   2. 현재 포인터가 사용자 주소인지
   3. 주소가 잘 매핑돼있는지 */
bool
validate_user_addr(void *ptr) {
	if (ptr == NULL)
		return false;

	if (!is_user_vaddr(ptr))
		return false;

	if (!pml4_get_page(thread_current()->pml4, ptr))
		return false;
	return true;
}