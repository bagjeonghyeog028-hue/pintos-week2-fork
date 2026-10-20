#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* 시스템 콜.
 *
 * 과거에는 인터럽트 핸들러가 시스템 콜을 처리했다
 * (예: Linux의 int 0x80). 그러나 x86-64에서는 제조사가
 * 시스템 콜을 효율적으로 요청할 수 있는 syscall 명령을 제공한다.
 *
 * syscall 명령은 모델별 레지스터(Model Specific Register, MSR)의
 * 값을 읽어 동작한다. 자세한 내용은 매뉴얼을 참고한다. */

#define MSR_STAR 0xc0000081         /* 세그먼트 선택자 MSR */
#define MSR_LSTAR 0xc0000082        /* 롱 모드 SYSCALL 진입 주소 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags 마스크 */

#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* syscall_entry가 사용자 스택을 커널 모드 스택으로 바꾸기 전까지는
	 * 인터럽트 서비스 루틴이 인터럽트를 처리하면 안 된다.
	 * 따라서 FLAG_FL을 마스킹한다. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}
/* 시스템 콜의 주 진입 인터페이스 */
void
syscall_handler (struct intr_frame *f) {
	// TODO: 여기에 구현을 작성한다.
	uint64_t input = f->R.rax;
	switch (input)
	{
	case SYS_HALT:
	{
		power_off();
		break;
	}
	case SYS_EXIT: //check
	{
		struct thread *t = thread_current();
		t->exit_status = f->R.rdi;
		
		thread_exit();
	}

	case SYS_FORK:
	{

		break;
	}

	case SYS_EXEC:
	{	
		struct thread *t = thread_current ();
		char *cmdline_copy = NULL; // 원래 명령을 복사해 둘 변수 -> 복사를 해서 원본은 보존하기 위해서 있는 복사본의 역할.
		char *argv[65];            // 나눈 단어를 담을 배열
		char *save_ptr;            // 다음 단어를 찾을 위치
		int argc = 0;              // 찾은 단어 수
		void *page;
		{
			/* data */
		};
		page = pml4_get_page(t->pml4,&save_ptr);
		if (page == NULL)
			return false;
		memcpy(page, argv, sizeof(argv));
	
		//todo 문자열 전체를 검사
		if (f->R.rdi == NULL)  {  //주소가 null인지 검사
			exit(-1);
		}
		if (f->R.rdi != NULL || is_user_vaddr((void *) f->R.rdi))  {   //true일시 복사후 전달.
			char t = f->R.rdi;
			process_exec(argv);
			exit(-1);
		} else {
		exit(-1);
		}
	break;
	}

	
	case SYS_WAIT: //check
		break;
	
	case SYS_CREATE:
	{
		f->R.rax = filesys_create(f->R.rdi, f->R.rsi);
		break;
	}

	case SYS_REMOVE:
	{
		f->R.rax = filesys_remove(f->R.rdi);
		break;
	}
	
	case SYS_OPEN:
		struct file *fl = filesys_open(f->R.rdi);
		break;

	case SYS_FILESIZE:
		break;

	case SYS_READ:
		break;

	case SYS_WRITE:
		break;

	case SYS_SEEK:
		break;

	case SYS_TELL:
		break;

	case SYS_CLOSE:
		break;
	
	default:
		f->R.rax = -1;
		return;
	}

}
