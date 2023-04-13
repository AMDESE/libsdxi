#ifndef _SAMPLES_H
#define _SAMPLES_H

inline static void posix_memalign_check(void ** ptr, size_t align, size_t size) {
	int ret = posix_memalign(ptr, align, size);

	switch (ret) {
	case 0:
		return;
	case EINVAL:
		fprintf(stderr, "posix_memalign: invalid argument\n");
	case ENOMEM:
		fprintf(stderr, "posix_memalign: insufficient memory\n");
	default:
		exit(1);
	}
}

#endif
