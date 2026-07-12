#include <sys/kcrt/kcrt.h>
#include <sys/hal/irq.h>

void
assert_impl(
	const char *file,
	int line,
	const char *exp)
{
	printf("\n"
	       "[Kernel Assertion Failed]\n"
	       "  file: %s\n"
	       "  line: %d\n"
	       "  expression: %s\n",
	       file,
	       line,
	       exp);

	/* Disable IRQ. */
	irq_acquire_lock();

	/* Halt. */
	while (1)
		;
}

void
fatal_impl(
	const char *file,
	int line,
	const char *s)
{
	printf("\n"
	       "[Kernel Fatal] %s\n"
	       "  file: %s\n"
	       "  line: %d\n",
	       s,
	       file,
	       line);

	/* Disable IRQ. */
	irq_acquire_lock();

	/* Halt. */
	while (1)
		;
}
