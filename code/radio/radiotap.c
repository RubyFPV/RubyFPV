#include "radiotap.h"
#include "../base/base.h"

/* Are two types/vars the same type (ignoring qualifiers)? */
#ifndef __same_type
# define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#endif

#ifndef BUILD_BUG_ON
/* Force a compilation error if condition is true */
#define BUILD_BUG_ON(condition) ((void)BUILD_BUG_ON_ZERO(condition))
/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void *)sizeof(struct { int:-!!(e); }))
#endif

#define __must_be_array(a)      BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

/*

int ieee80211_radiotap_iterator_init(struct ieee80211_radiotap_iterator *iterator, struct ieee80211_radiotap_header *radiotap_header, int max_length)
{
 if (radiotap_header->it_version)
		return -EINVAL;

	if (max_length < le16_to_cpu(radiotap_header->it_len))
		return -EINVAL;

	iterator->rtheader = radiotap_header;
	iterator->max_length = le16_to_cpu(radiotap_header->it_len);
	iterator->arg_index = 0;
	iterator->bitmap_shifter = le32_to_cpu(radiotap_header->it_present);
	iterator->arg = (u8 *)radiotap_header + sizeof(*radiotap_header);
	iterator->this_arg = 0;

	if (unlikely(iterator->bitmap_shifter & (1<<IEEE80211_RADIOTAP_EXT))) {
		while (le32_to_cpu(*((u32 *)iterator->arg)) &
				   (1<<IEEE80211_RADIOTAP_EXT)) {
			iterator->arg += sizeof(u32);

			if (((ulong)iterator->arg -
			     (ulong)iterator->rtheader) > iterator->max_length)
				return -EINVAL;
		}

		iterator->arg += sizeof(u32);
	}

	return 0;
}
*/


static const struct radiotap_align_size rtap_namespace_sizes[] = {
 [IEEE80211_RADIOTAP_TSFT] = { .align = 8, .size = 8, },
 [IEEE80211_RADIOTAP_FLAGS] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_RATE] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_CHANNEL] = { .align = 2, .size = 4, },
 [IEEE80211_RADIOTAP_FHSS] = { .align = 2, .size = 2, },
 [IEEE80211_RADIOTAP_DBM_ANTSIGNAL] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_DBM_ANTNOISE] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_LOCK_QUALITY] = { .align = 2, .size = 2, },
 [IEEE80211_RADIOTAP_TX_ATTENUATION] = { .align = 2, .size = 2, },
 [IEEE80211_RADIOTAP_DB_TX_ATTENUATION] = { .align = 2, .size = 2, },
 [IEEE80211_RADIOTAP_DBM_TX_POWER] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_ANTENNA] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_DB_ANTSIGNAL] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_DB_ANTNOISE] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_RX_FLAGS] = { .align = 2, .size = 2, },
 [IEEE80211_RADIOTAP_TX_FLAGS] = { .align = 2, .size = 2, },
 [IEEE80211_RADIOTAP_RTS_RETRIES] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_DATA_RETRIES] = { .align = 1, .size = 1, },
 [IEEE80211_RADIOTAP_MCS] = { .align = 1, .size = 3, },
 [IEEE80211_RADIOTAP_AMPDU_STATUS] = { .align = 4, .size = 8, },
 [IEEE80211_RADIOTAP_VHT] = { .align = 2, .size = 12, },
 [IEEE80211_RADIOTAP_TIMESTAMP] = { .align = 8, .size = 12, },
};

static const struct ieee80211_radiotap_namespace radiotap_ns_default = {
 .n_bits = ARRAY_SIZE(rtap_namespace_sizes),
 .align_size = rtap_namespace_sizes,
};


int ieee80211_radiotap_iterator_init(struct ieee80211_radiotap_iterator *iterator, struct ieee80211_radiotap_header *radiotap_header, int max_length)
{
 if (max_length < (int)sizeof(struct ieee80211_radiotap_header))
  return -EINVAL;

 if (radiotap_header->it_version)
  return -EINVAL;

 if (max_length < get_unaligned_le16(&radiotap_header->it_len))
  return -EINVAL;

 iterator->_rtheader = radiotap_header;
 iterator->max_length = get_unaligned_le16(&radiotap_header->it_len);
 iterator->_arg_index = 0;
 iterator->_bitmap_shifter = get_unaligned_le32(&radiotap_header->it_present);
 iterator->_arg = (uint8_t *)radiotap_header + sizeof(*radiotap_header);
 iterator->_reset_on_ext = 0;
 iterator->_next_bitmap = &radiotap_header->it_present;
 iterator->_next_bitmap++;
 iterator->_vns = NULL;
 iterator->current_namespace = &radiotap_ns_default;
 iterator->is_radiotap_ns = 1;

 if (iterator->_bitmap_shifter & (1<<IEEE80211_RADIOTAP_EXT)) {
  if ((unsigned long)iterator->_arg -
      (unsigned long)iterator->_rtheader + sizeof(uint32_t) >
      (unsigned long)iterator->max_length)
   return -EINVAL;
  while (get_unaligned_le32(iterator->_arg) &
     (1 << IEEE80211_RADIOTAP_EXT)) {
   iterator->_arg += sizeof(uint32_t);

   if ((unsigned long)iterator->_arg -
       (unsigned long)iterator->_rtheader +
       sizeof(uint32_t) >
       (unsigned long)iterator->max_length)
    return -EINVAL;
  }

  iterator->_arg += sizeof(uint32_t);
 }

 iterator->this_arg = iterator->_arg;

 return 0;
}

/*
int ieee80211_radiotap_iterator_next(
    struct ieee80211_radiotap_iterator *iterator)
{

	static const u8 rt_sizes[] = {
		[IEEE80211_RADIOTAP_TSFT] = 0x88,
		[IEEE80211_RADIOTAP_FLAGS] = 0x11,
		[IEEE80211_RADIOTAP_RATE] = 0x11,
		[IEEE80211_RADIOTAP_CHANNEL] = 0x24,
		[IEEE80211_RADIOTAP_FHSS] = 0x22,
		[IEEE80211_RADIOTAP_DBM_ANTSIGNAL] = 0x11,
		[IEEE80211_RADIOTAP_DBM_ANTNOISE] = 0x11,
		[IEEE80211_RADIOTAP_LOCK_QUALITY] = 0x22,
		[IEEE80211_RADIOTAP_TX_ATTENUATION] = 0x22,
		[IEEE80211_RADIOTAP_DB_TX_ATTENUATION] = 0x22,
		[IEEE80211_RADIOTAP_DBM_TX_POWER] = 0x11,
		[IEEE80211_RADIOTAP_ANTENNA] = 0x11,
		[IEEE80211_RADIOTAP_DB_ANTSIGNAL] = 0x11,
		[IEEE80211_RADIOTAP_DB_ANTNOISE] = 0x11,
  [IEEE80211_RADIOTAP_RX_FLAGS] = 0x22,
  [IEEE80211_RADIOTAP_TX_FLAGS] = 0x22,
  [IEEE80211_RADIOTAP_RTS_RETRIES] = 0x11,
  [IEEE80211_RADIOTAP_DATA_RETRIES] = 0x11
  //[18] = 0x11,
  //[IEEE80211_RADIOTAP_MCS] = 0x13

	};



	while (iterator->arg_index < sizeof(rt_sizes)) {
		int hit = 0;
		int pad;

		if (!(iterator->bitmap_shifter & 1))
			goto next_entry;


		pad = (((ulong)iterator->arg) -
			((ulong)iterator->rtheader)) &
			((rt_sizes[iterator->arg_index] >> 4) - 1);

		if (pad)
			iterator->arg +=
				(rt_sizes[iterator->arg_index] >> 4) - pad;


		iterator->this_arg_index = iterator->arg_index;
		iterator->this_arg = iterator->arg;
		hit = 1;

		iterator->arg += rt_sizes[iterator->arg_index] & 0x0f;



		if (((ulong)iterator->arg - (ulong)iterator->rtheader) >
		    iterator->max_length)
			return -EINVAL;

	next_entry:
		iterator->arg_index++;
		if (unlikely((iterator->arg_index & 31) == 0)) {

			if (iterator->bitmap_shifter & 1) {

				iterator->bitmap_shifter =
				    le32_to_cpu(*iterator->next_bitmap);
				iterator->next_bitmap++;
			} else {

				iterator->arg_index = sizeof(rt_sizes);
			}
		} else {
			iterator->bitmap_shifter >>= 1;
		}


		if (hit)
			return 0;
	}

	return -ENOENT;
}
*/

static void find_ns(struct ieee80211_radiotap_iterator *iterator,
      uint32_t oui, uint8_t subns)
{
 int i;

 iterator->current_namespace = NULL;

 if (!iterator->_vns)
  return;

 for (i = 0; i < iterator->_vns->n_ns; i++) {
  if (iterator->_vns->ns[i].oui != oui)
   continue;
  if (iterator->_vns->ns[i].subns != subns)
   continue;

  iterator->current_namespace = &iterator->_vns->ns[i];
  break;
 }
}

extern int ieee80211_radiotap_iterator_next(struct ieee80211_radiotap_iterator *iterator)
{
   while (1) {
  int hit = 0;
  int pad, align, size, subns;
  uint32_t oui;

  if ((iterator->_arg_index % 32) == IEEE80211_RADIOTAP_EXT &&
      !(iterator->_bitmap_shifter & 1))
   return -ENOENT;

  if (!(iterator->_bitmap_shifter & 1))
   goto next_entry;

  switch (iterator->_arg_index % 32) {
  case IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE:
  case IEEE80211_RADIOTAP_EXT:
   align = 1;
   size = 0;
   break;
  case IEEE80211_RADIOTAP_VENDOR_NAMESPACE:
   align = 2;
   size = 6;
   break;
  default:
   if (!iterator->current_namespace ||
       iterator->_arg_index >= iterator->current_namespace->n_bits) {
    if (iterator->current_namespace == &radiotap_ns_default)
     return -ENOENT;
    align = 0;
   } else {
    align = iterator->current_namespace->align_size[iterator->_arg_index].align;
    size = iterator->current_namespace->align_size[iterator->_arg_index].size;
   }
   if (!align) {
    iterator->_arg = iterator->_next_ns_data;
    iterator->current_namespace = NULL;
    goto next_entry;
   }
   break;
  }

  pad = ((unsigned long)iterator->_arg -
         (unsigned long)iterator->_rtheader) & (align - 1);

  if (pad)
   iterator->_arg += align - pad;

  if (iterator->_arg_index % 32 == IEEE80211_RADIOTAP_VENDOR_NAMESPACE) {
   int vnslen;

   if ((unsigned long)iterator->_arg + size -
       (unsigned long)iterator->_rtheader >
       (unsigned long)iterator->max_length)
    return -EINVAL;

   oui = (*iterator->_arg << 16) |
    (*(iterator->_arg + 1) << 8) |
    *(iterator->_arg + 2);
   subns = *(iterator->_arg + 3);

   find_ns(iterator, oui, subns);

   vnslen = get_unaligned_le16(iterator->_arg + 4);
   iterator->_next_ns_data = iterator->_arg + size + vnslen;
   if (!iterator->current_namespace)
    size += vnslen;
  }

  iterator->this_arg_index = iterator->_arg_index;
  iterator->this_arg = iterator->_arg;
  iterator->this_arg_size = size;

  iterator->_arg += size;

  if ((unsigned long)iterator->_arg -
      (unsigned long)iterator->_rtheader >
      (unsigned long)iterator->max_length)
   return -EINVAL;

  switch (iterator->_arg_index % 32) {
  case IEEE80211_RADIOTAP_VENDOR_NAMESPACE:
   iterator->_reset_on_ext = 1;

   iterator->is_radiotap_ns = 0;
   iterator->this_arg_index =
    IEEE80211_RADIOTAP_VENDOR_NAMESPACE;
   if (!iterator->current_namespace)
    hit = 1;
   goto next_entry;
  case IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE:
   iterator->_reset_on_ext = 1;
   iterator->current_namespace = &radiotap_ns_default;
   iterator->is_radiotap_ns = 1;
   goto next_entry;
  case IEEE80211_RADIOTAP_EXT:
   iterator->_bitmap_shifter =
    get_unaligned_le32(iterator->_next_bitmap);
   iterator->_next_bitmap++;
   if (iterator->_reset_on_ext)
    iterator->_arg_index = 0;
   else
    iterator->_arg_index++;
   iterator->_reset_on_ext = 0;
   break;
  default:
   hit = 1;
 next_entry:
   iterator->_bitmap_shifter >>= 1;
   iterator->_arg_index++;
  }

  if (hit)
   return 0;
 }
}
