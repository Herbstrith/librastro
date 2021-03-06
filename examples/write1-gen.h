/* Do not edit. File generated by rastro. */

#ifndef __AUTO_RASTRO_FILE_H__
#define __AUTO_RASTRO_FILE_H__
#include <rastro.h>

/* Rastro function prototype for 'is' */
void rst_event_is_ptr(rst_buffer_t *ptr, u_int16_t type, u_int32_t i0, const char* s0);
void rst_event_is_f_ (int16_t *type, int32_t* i0, const char* s0);
#define rst_event_is(type, i0, s0) rst_event_is_ptr(RST_PTR, type, i0, s0)

/* Rastro function prototype for 'is' */
void rst_event_is_ptr(rst_buffer_t *ptr, u_int16_t type, u_int32_t i0, const char* s0);
void rst_event_is_f_ (int16_t *type, int32_t* i0, const char* s0);
#define rst_event_is(type, i0, s0) rst_event_is_ptr(RST_PTR, type, i0, s0)

/* Rastro function prototype for 'is' */
void rst_event_is_ptr(rst_buffer_t *ptr, u_int16_t type, u_int32_t i0, const char* s0);
void rst_event_is_f_ (int16_t *type, int32_t* i0, const char* s0);
#define rst_event_is(type, i0, s0) rst_event_is_ptr(RST_PTR, type, i0, s0)

/* Rastro function prototype for 'will' */
void rst_event_will_ptr(rst_buffer_t *ptr, u_int16_t type, u_int16_t w0, u_int32_t i0, u_int64_t l0, u_int64_t l1);
void rst_event_will_f_ (int16_t *type, int16_t* w0, int32_t* i0, int64_t* l0, int64_t* l1);
#define rst_event_will(type, w0, i0, l0, l1) rst_event_will_ptr(RST_PTR, type, w0, i0, l0, l1)

/* Rastro function prototype for 'if' */
void rst_event_if_ptr(rst_buffer_t *ptr, u_int16_t type, u_int32_t i0, float f0);
void rst_event_if_f_ (int16_t *type, int32_t* i0, float* f0);
#define rst_event_if(type, i0, f0) rst_event_if_ptr(RST_PTR, type, i0, f0)

/* Rastro function prototype for 'i' */
void rst_event_i_ptr(rst_buffer_t *ptr, u_int16_t type, u_int32_t i0);
void rst_event_i_f_ (int16_t *type, int32_t* i0);
#define rst_event_i(type, i0) rst_event_i_ptr(RST_PTR, type, i0)

void rst_init_f_(int64_t *id1, int64_t *id2);
void rst_finalize_f_ (void);

#endif //__AUTO_RASTRO_FILE_H__
