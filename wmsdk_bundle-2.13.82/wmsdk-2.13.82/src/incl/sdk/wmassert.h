#ifndef _WM_ASSERT_H_
#define _WM_ASSERT_H_


#ifdef CONFIG_ENABLE_ASSERTS
#define ASSERT(_cond_) if(!(_cond_)) \
		_wm_assert(__FILE__, __LINE__, #_cond_)
#else /* ! CONFIG_ENABLE_ASSERTS */
#define ASSERT(_cond_)
#endif /* CONFIG_ENABLE_ASSERTS */


void _wm_assert(const char *filename, int lineno, 
	    const char* fail_cond);


#endif // _WM_ASSERT_H_
