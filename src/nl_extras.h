#ifndef __NL_EXTRAS_H
#define __NL_EXTRAS_H

#if LIBNL_VER_MIC <= 26

#ifndef NLA_S8

#define NLA_S8	13
#define NLA_PUT_S8(n, attrtype, value) \
	NLA_PUT_TYPE(n, int8_t, attrtype, value)

static inline int8_t nla_get_s8(struct nlattr *nla)
{
	return *(int8_t *) nla_data(nla);
}

#endif /* NLA_S8 */

#ifndef NLA_S16

#define NLA_S16	14
#define NLA_PUT_S16(n, attrtype, value) \
	NLA_PUT_TYPE(n, int16_t, attrtype, value)

#endif /* NLA_S16 */

#ifndef NLA_S32

#define NLA_S32	15
#define NLA_PUT_S32(n, attrtype, value) \
	NLA_PUT_TYPE(n, int32_t, attrtype, value)

static inline int32_t nla_get_s32(struct nlattr *nla)
{
	return *(int32_t *) nla_data(nla);
}

#endif /* NLA_S32 */

#ifndef NLA_S64

#define NLA_S64	16
#define NLA_PUT_S64(n, attrtype, value) \
	NLA_PUT_TYPE(n, int64_t, attrtype, value)

#endif /* NLA_S64 */

#endif /* LIBNL_VER_MIC */

#endif /* __NL_EXTRAS_H */
