/* ==========================================================================
 *  AURA :: theme.h  --  the single source of truth for the visual language
 *
 *  "Amethyst & Bone": a violet/white identity built for an airport operations
 *  floor.  Light surfaces carry the paperwork; deep violet carries the live
 *  airfield.  Every colour in the product resolves to a token below.
 * ========================================================================== */
#ifndef AURA_THEME_H
#define AURA_THEME_H

#include "../src/engine/canvas.h"

/* ---- ink ---------------------------------------------------------------- */
#define C_INK        HEX(0x1B0F33)   /* primary type                         */
#define C_INK_2      HEX(0x53406F)   /* secondary type                       */
#define C_INK_3      HEX(0x8B7BA8)   /* tertiary / captions                  */
#define C_INK_4      HEX(0xB6A9CC)   /* disabled                             */

/* ---- surfaces ----------------------------------------------------------- */
#define C_BG         HEX(0xF6F3FD)   /* application backdrop                 */
#define C_SURF       HEX(0xFFFFFF)   /* cards                                */
#define C_SURF_2     HEX(0xF3EEFE)   /* sunken wells                         */
#define C_SURF_3     HEX(0xEAE2FB)   /* pressed / track                      */
#define C_LINE       HEX(0xE7DFF9)   /* hairlines                            */
#define C_LINE_2     HEX(0xD6C9F2)   /* stronger borders                     */

/* ---- violet ramp -------------------------------------------------------- */
#define C_V50        HEX(0xF5F1FF)
#define C_V100       HEX(0xEDE7FE)
#define C_V200       HEX(0xDDD2FD)
#define C_V300       HEX(0xC3B0FB)
#define C_V400       HEX(0xA383F6)
#define C_V500       HEX(0x8355EC)
#define C_V600       HEX(0x6E35DC)
#define C_V700       HEX(0x5B27B8)
#define C_V800       HEX(0x461C8E)
#define C_V900       HEX(0x2F1263)
#define C_V950       HEX(0x1D0A42)

#define C_BRAND      C_V600
#define C_BRAND_HI   C_V500

/* ---- night (airfield / radar surfaces) ---------------------------------- */
#define C_NIGHT      HEX(0x120726)
#define C_NIGHT_2    HEX(0x1B0C39)
#define C_NIGHT_3    HEX(0x261252)
#define C_NIGHT_LINE HEX(0x361C6B)

/* ---- status ------------------------------------------------------------- */
#define C_OK         HEX(0x12B76A)
#define C_OK_BG      HEX(0xE6F8F0)
#define C_WARN       HEX(0xF59E0B)
#define C_WARN_BG    HEX(0xFEF3E2)
#define C_DANGER     HEX(0xE5484D)
#define C_DANGER_BG  HEX(0xFDEBEC)
#define C_INFO       HEX(0x3B82F6)
#define C_INFO_BG    HEX(0xE8F1FE)
#define C_TEAL       HEX(0x0EA5A5)
#define C_MAGENTA    HEX(0xC02BC7)
#define C_GOLD       HEX(0xD9A521)

/* ---- geometry ----------------------------------------------------------- */
#define R_SM   6.f
#define R_MD   10.f
#define R_LG   14.f
#define R_XL   20.f
#define R_PILL 999.f

#define SIDEBAR_W   232.f
#define TOPBAR_H    68.f
#define PAD         20.f

/* ---- motion ------------------------------------------------------------- */
#define SPD_FAST   18.f
#define SPD_MED    11.f
#define SPD_SLOW    6.f

#endif /* AURA_THEME_H */
