/*
 * Copyright (c) 2014, Michael Müller
 * Copyright (c) 2014, Sebastian Lackner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _H_KEYBOARD_
#define _H_KEYBOARD_

#include <stdbool.h>
#include <stdint.h>

#define KEY_CODE(type, value)	(((type)<<8)|(value))
#define KEY_TYPE(x)				(((x) >> 8))
#define KEY_VALUE(x)			((x) & 0xff)

#define KEY_MODIFIER_SHIFT        0
#define KEY_MODIFIER_ALTGR        1
#define KEY_MODIFIER_CTRL         2
#define KEY_MODIFIER_ALT          3
#define KEY_MODIFIER_SHIFTL       4
#define KEY_MODIFIER_KANASHIFT    4
#define KEY_MODIFIER_SHIFTR       5
#define KEY_MODIFIER_CTRLL        6
#define KEY_MODIFIER_CTRLR        7
#define KEY_MODIFIER_CAPSSHIFT    8

#define KEY_TYPE_LATIN	0x0
#define KEY_TYPE_FN		0x1
#define KEY_TYPE_SPEC	0x2
#define KEY_TYPE_PAD	0x3
#define KEY_TYPE_DEAD	0x4
#define KEY_TYPE_CONS	0x5
#define KEY_TYPE_CUR	0x6
#define KEY_TYPE_SHIFT	0x7
#define KEY_TYPE_META	0x8
#define KEY_TYPE_ASCII  0x9
#define KEY_TYPE_LOCK	0xA
#define KEY_TYPE_LETTER 0xB
#define KEY_TYPE_SLOCK	0xC
#define KEY_TYPE_DEAD2	0xD
#define KEY_TYPE_BRL	0xE

#define KEY_CODE_F1            KEY_CODE(KEY_TYPE_FN, 0)
#define KEY_CODE_F2            KEY_CODE(KEY_TYPE_FN, 1)
#define KEY_CODE_F3            KEY_CODE(KEY_TYPE_FN, 2)
#define KEY_CODE_F4            KEY_CODE(KEY_TYPE_FN, 3)
#define KEY_CODE_F5            KEY_CODE(KEY_TYPE_FN, 4)
#define KEY_CODE_F6            KEY_CODE(KEY_TYPE_FN, 5)
#define KEY_CODE_F7            KEY_CODE(KEY_TYPE_FN, 6)
#define KEY_CODE_F8            KEY_CODE(KEY_TYPE_FN, 7)
#define KEY_CODE_F9            KEY_CODE(KEY_TYPE_FN, 8)
#define KEY_CODE_F10           KEY_CODE(KEY_TYPE_FN, 9)
#define KEY_CODE_F11           KEY_CODE(KEY_TYPE_FN, 10)
#define KEY_CODE_F12           KEY_CODE(KEY_TYPE_FN, 11)
#define KEY_CODE_F13           KEY_CODE(KEY_TYPE_FN, 12)
#define KEY_CODE_F14           KEY_CODE(KEY_TYPE_FN, 13)
#define KEY_CODE_F15           KEY_CODE(KEY_TYPE_FN, 14)
#define KEY_CODE_F16           KEY_CODE(KEY_TYPE_FN, 15)
#define KEY_CODE_F17           KEY_CODE(KEY_TYPE_FN, 16)
#define KEY_CODE_F18           KEY_CODE(KEY_TYPE_FN, 17)
#define KEY_CODE_F19           KEY_CODE(KEY_TYPE_FN, 18)
#define KEY_CODE_F20           KEY_CODE(KEY_TYPE_FN, 19)
#define KEY_CODE_FIND          KEY_CODE(KEY_TYPE_FN, 20)
#define KEY_CODE_INSERT        KEY_CODE(KEY_TYPE_FN, 21)
#define KEY_CODE_REMOVE        KEY_CODE(KEY_TYPE_FN, 22)
#define KEY_CODE_SELECT        KEY_CODE(KEY_TYPE_FN, 23)
#define KEY_CODE_PGUP          KEY_CODE(KEY_TYPE_FN, 24)
#define KEY_CODE_PGDN          KEY_CODE(KEY_TYPE_FN, 25)
#define KEY_CODE_MACRO         KEY_CODE(KEY_TYPE_FN, 26)
#define KEY_CODE_HELP          KEY_CODE(KEY_TYPE_FN, 27)
#define KEY_CODE_DO            KEY_CODE(KEY_TYPE_FN, 28)
#define KEY_CODE_PAUSE         KEY_CODE(KEY_TYPE_FN, 29)
#define KEY_CODE_F21           KEY_CODE(KEY_TYPE_FN, 30)
#define KEY_CODE_F22           KEY_CODE(KEY_TYPE_FN, 31)
#define KEY_CODE_F23           KEY_CODE(KEY_TYPE_FN, 32)
#define KEY_CODE_F24           KEY_CODE(KEY_TYPE_FN, 33)
#define KEY_CODE_F25           KEY_CODE(KEY_TYPE_FN, 34)
#define KEY_CODE_F26           KEY_CODE(KEY_TYPE_FN, 35)
#define KEY_CODE_F27           KEY_CODE(KEY_TYPE_FN, 36)
#define KEY_CODE_F28           KEY_CODE(KEY_TYPE_FN, 37)
#define KEY_CODE_F29           KEY_CODE(KEY_TYPE_FN, 38)
#define KEY_CODE_F30           KEY_CODE(KEY_TYPE_FN, 39)
#define KEY_CODE_F31           KEY_CODE(KEY_TYPE_FN, 40)
#define KEY_CODE_F32           KEY_CODE(KEY_TYPE_FN, 41)
#define KEY_CODE_F33           KEY_CODE(KEY_TYPE_FN, 42)
#define KEY_CODE_F34           KEY_CODE(KEY_TYPE_FN, 43)
#define KEY_CODE_F35           KEY_CODE(KEY_TYPE_FN, 44)
#define KEY_CODE_F36           KEY_CODE(KEY_TYPE_FN, 45)
#define KEY_CODE_F37           KEY_CODE(KEY_TYPE_FN, 46)
#define KEY_CODE_F38           KEY_CODE(KEY_TYPE_FN, 47)
#define KEY_CODE_F39           KEY_CODE(KEY_TYPE_FN, 48)
#define KEY_CODE_F40           KEY_CODE(KEY_TYPE_FN, 49)
#define KEY_CODE_F41           KEY_CODE(KEY_TYPE_FN, 50)
#define KEY_CODE_F42           KEY_CODE(KEY_TYPE_FN, 51)
#define KEY_CODE_F43           KEY_CODE(KEY_TYPE_FN, 52)
#define KEY_CODE_F44           KEY_CODE(KEY_TYPE_FN, 53)
#define KEY_CODE_F45           KEY_CODE(KEY_TYPE_FN, 54)
#define KEY_CODE_F46           KEY_CODE(KEY_TYPE_FN, 55)
#define KEY_CODE_F47           KEY_CODE(KEY_TYPE_FN, 56)
#define KEY_CODE_F48           KEY_CODE(KEY_TYPE_FN, 57)
#define KEY_CODE_F49           KEY_CODE(KEY_TYPE_FN, 58)
#define KEY_CODE_F50           KEY_CODE(KEY_TYPE_FN, 59)
#define KEY_CODE_F51           KEY_CODE(KEY_TYPE_FN, 60)
#define KEY_CODE_F52           KEY_CODE(KEY_TYPE_FN, 61)
#define KEY_CODE_F53           KEY_CODE(KEY_TYPE_FN, 62)
#define KEY_CODE_F54           KEY_CODE(KEY_TYPE_FN, 63)
#define KEY_CODE_F55           KEY_CODE(KEY_TYPE_FN, 64)
#define KEY_CODE_F56           KEY_CODE(KEY_TYPE_FN, 65)
#define KEY_CODE_F57           KEY_CODE(KEY_TYPE_FN, 66)
#define KEY_CODE_F58           KEY_CODE(KEY_TYPE_FN, 67)
#define KEY_CODE_F59           KEY_CODE(KEY_TYPE_FN, 68)
#define KEY_CODE_F60           KEY_CODE(KEY_TYPE_FN, 69)
#define KEY_CODE_F61           KEY_CODE(KEY_TYPE_FN, 70)
#define KEY_CODE_F62           KEY_CODE(KEY_TYPE_FN, 71)
#define KEY_CODE_F63           KEY_CODE(KEY_TYPE_FN, 72)
#define KEY_CODE_F64           KEY_CODE(KEY_TYPE_FN, 73)
#define KEY_CODE_F65           KEY_CODE(KEY_TYPE_FN, 74)
#define KEY_CODE_F66           KEY_CODE(KEY_TYPE_FN, 75)
#define KEY_CODE_F67           KEY_CODE(KEY_TYPE_FN, 76)
#define KEY_CODE_F68           KEY_CODE(KEY_TYPE_FN, 77)
#define KEY_CODE_F69           KEY_CODE(KEY_TYPE_FN, 78)
#define KEY_CODE_F70           KEY_CODE(KEY_TYPE_FN, 79)
#define KEY_CODE_F71           KEY_CODE(KEY_TYPE_FN, 80)
#define KEY_CODE_F72           KEY_CODE(KEY_TYPE_FN, 81)
#define KEY_CODE_F73           KEY_CODE(KEY_TYPE_FN, 82)
#define KEY_CODE_F74           KEY_CODE(KEY_TYPE_FN, 83)
#define KEY_CODE_F75           KEY_CODE(KEY_TYPE_FN, 84)
#define KEY_CODE_F76           KEY_CODE(KEY_TYPE_FN, 85)
#define KEY_CODE_F77           KEY_CODE(KEY_TYPE_FN, 86)
#define KEY_CODE_F78           KEY_CODE(KEY_TYPE_FN, 87)
#define KEY_CODE_F79           KEY_CODE(KEY_TYPE_FN, 88)
#define KEY_CODE_F80           KEY_CODE(KEY_TYPE_FN, 89)
#define KEY_CODE_F81           KEY_CODE(KEY_TYPE_FN, 90)
#define KEY_CODE_F82           KEY_CODE(KEY_TYPE_FN, 91)
#define KEY_CODE_F83           KEY_CODE(KEY_TYPE_FN, 92)
#define KEY_CODE_F84           KEY_CODE(KEY_TYPE_FN, 93)
#define KEY_CODE_F85           KEY_CODE(KEY_TYPE_FN, 94)
#define KEY_CODE_F86           KEY_CODE(KEY_TYPE_FN, 95)
#define KEY_CODE_F87           KEY_CODE(KEY_TYPE_FN, 96)
#define KEY_CODE_F88           KEY_CODE(KEY_TYPE_FN, 97)
#define KEY_CODE_F89           KEY_CODE(KEY_TYPE_FN, 98)
#define KEY_CODE_F90           KEY_CODE(KEY_TYPE_FN, 99)
#define KEY_CODE_F91           KEY_CODE(KEY_TYPE_FN, 100)
#define KEY_CODE_F92           KEY_CODE(KEY_TYPE_FN, 101)
#define KEY_CODE_F93           KEY_CODE(KEY_TYPE_FN, 102)
#define KEY_CODE_F94           KEY_CODE(KEY_TYPE_FN, 103)
#define KEY_CODE_F95           KEY_CODE(KEY_TYPE_FN, 104)
#define KEY_CODE_F96           KEY_CODE(KEY_TYPE_FN, 105)
#define KEY_CODE_F97           KEY_CODE(KEY_TYPE_FN, 106)
#define KEY_CODE_F98           KEY_CODE(KEY_TYPE_FN, 107)
#define KEY_CODE_F99           KEY_CODE(KEY_TYPE_FN, 108)
#define KEY_CODE_F100          KEY_CODE(KEY_TYPE_FN, 109)
#define KEY_CODE_F101          KEY_CODE(KEY_TYPE_FN, 110)
#define KEY_CODE_F102          KEY_CODE(KEY_TYPE_FN, 111)
#define KEY_CODE_F103          KEY_CODE(KEY_TYPE_FN, 112)
#define KEY_CODE_F104          KEY_CODE(KEY_TYPE_FN, 113)
#define KEY_CODE_F105          KEY_CODE(KEY_TYPE_FN, 114)
#define KEY_CODE_F106          KEY_CODE(KEY_TYPE_FN, 115)
#define KEY_CODE_F107          KEY_CODE(KEY_TYPE_FN, 116)
#define KEY_CODE_F108          KEY_CODE(KEY_TYPE_FN, 117)
#define KEY_CODE_F109          KEY_CODE(KEY_TYPE_FN, 118)
#define KEY_CODE_F110          KEY_CODE(KEY_TYPE_FN, 119)
#define KEY_CODE_F111          KEY_CODE(KEY_TYPE_FN, 120)
#define KEY_CODE_F112          KEY_CODE(KEY_TYPE_FN, 121)
#define KEY_CODE_F113          KEY_CODE(KEY_TYPE_FN, 122)
#define KEY_CODE_F114          KEY_CODE(KEY_TYPE_FN, 123)
#define KEY_CODE_F115          KEY_CODE(KEY_TYPE_FN, 124)
#define KEY_CODE_F116          KEY_CODE(KEY_TYPE_FN, 125)
#define KEY_CODE_F117          KEY_CODE(KEY_TYPE_FN, 126)
#define KEY_CODE_F118          KEY_CODE(KEY_TYPE_FN, 127)
#define KEY_CODE_F119          KEY_CODE(KEY_TYPE_FN, 128)
#define KEY_CODE_F120          KEY_CODE(KEY_TYPE_FN, 129)
#define KEY_CODE_F121          KEY_CODE(KEY_TYPE_FN, 130)
#define KEY_CODE_F122          KEY_CODE(KEY_TYPE_FN, 131)
#define KEY_CODE_F123          KEY_CODE(KEY_TYPE_FN, 132)
#define KEY_CODE_F124          KEY_CODE(KEY_TYPE_FN, 133)
#define KEY_CODE_F125          KEY_CODE(KEY_TYPE_FN, 134)
#define KEY_CODE_F126          KEY_CODE(KEY_TYPE_FN, 135)
#define KEY_CODE_F127          KEY_CODE(KEY_TYPE_FN, 136)
#define KEY_CODE_F128          KEY_CODE(KEY_TYPE_FN, 137)
#define KEY_CODE_F129          KEY_CODE(KEY_TYPE_FN, 138)
#define KEY_CODE_F130          KEY_CODE(KEY_TYPE_FN, 139)
#define KEY_CODE_F131          KEY_CODE(KEY_TYPE_FN, 140)
#define KEY_CODE_F132          KEY_CODE(KEY_TYPE_FN, 141)
#define KEY_CODE_F133          KEY_CODE(KEY_TYPE_FN, 142)
#define KEY_CODE_F134          KEY_CODE(KEY_TYPE_FN, 143)
#define KEY_CODE_F135          KEY_CODE(KEY_TYPE_FN, 144)
#define KEY_CODE_F136          KEY_CODE(KEY_TYPE_FN, 145)
#define KEY_CODE_F137          KEY_CODE(KEY_TYPE_FN, 146)
#define KEY_CODE_F138          KEY_CODE(KEY_TYPE_FN, 147)
#define KEY_CODE_F139          KEY_CODE(KEY_TYPE_FN, 148)
#define KEY_CODE_F140          KEY_CODE(KEY_TYPE_FN, 149)
#define KEY_CODE_F141          KEY_CODE(KEY_TYPE_FN, 150)
#define KEY_CODE_F142          KEY_CODE(KEY_TYPE_FN, 151)
#define KEY_CODE_F143          KEY_CODE(KEY_TYPE_FN, 152)
#define KEY_CODE_F144          KEY_CODE(KEY_TYPE_FN, 153)
#define KEY_CODE_F145          KEY_CODE(KEY_TYPE_FN, 154)
#define KEY_CODE_F146          KEY_CODE(KEY_TYPE_FN, 155)
#define KEY_CODE_F147          KEY_CODE(KEY_TYPE_FN, 156)
#define KEY_CODE_F148          KEY_CODE(KEY_TYPE_FN, 157)
#define KEY_CODE_F149          KEY_CODE(KEY_TYPE_FN, 158)
#define KEY_CODE_F150          KEY_CODE(KEY_TYPE_FN, 159)
#define KEY_CODE_F151          KEY_CODE(KEY_TYPE_FN, 160)
#define KEY_CODE_F152          KEY_CODE(KEY_TYPE_FN, 161)
#define KEY_CODE_F153          KEY_CODE(KEY_TYPE_FN, 162)
#define KEY_CODE_F154          KEY_CODE(KEY_TYPE_FN, 163)
#define KEY_CODE_F155          KEY_CODE(KEY_TYPE_FN, 164)
#define KEY_CODE_F156          KEY_CODE(KEY_TYPE_FN, 165)
#define KEY_CODE_F157          KEY_CODE(KEY_TYPE_FN, 166)
#define KEY_CODE_F158          KEY_CODE(KEY_TYPE_FN, 167)
#define KEY_CODE_F159          KEY_CODE(KEY_TYPE_FN, 168)
#define KEY_CODE_F160          KEY_CODE(KEY_TYPE_FN, 169)
#define KEY_CODE_F161          KEY_CODE(KEY_TYPE_FN, 170)
#define KEY_CODE_F162          KEY_CODE(KEY_TYPE_FN, 171)
#define KEY_CODE_F163          KEY_CODE(KEY_TYPE_FN, 172)
#define KEY_CODE_F164          KEY_CODE(KEY_TYPE_FN, 173)
#define KEY_CODE_F165          KEY_CODE(KEY_TYPE_FN, 174)
#define KEY_CODE_F166          KEY_CODE(KEY_TYPE_FN, 175)
#define KEY_CODE_F167          KEY_CODE(KEY_TYPE_FN, 176)
#define KEY_CODE_F168          KEY_CODE(KEY_TYPE_FN, 177)
#define KEY_CODE_F169          KEY_CODE(KEY_TYPE_FN, 178)
#define KEY_CODE_F170          KEY_CODE(KEY_TYPE_FN, 179)
#define KEY_CODE_F171          KEY_CODE(KEY_TYPE_FN, 180)
#define KEY_CODE_F172          KEY_CODE(KEY_TYPE_FN, 181)
#define KEY_CODE_F173          KEY_CODE(KEY_TYPE_FN, 182)
#define KEY_CODE_F174          KEY_CODE(KEY_TYPE_FN, 183)
#define KEY_CODE_F175          KEY_CODE(KEY_TYPE_FN, 184)
#define KEY_CODE_F176          KEY_CODE(KEY_TYPE_FN, 185)
#define KEY_CODE_F177          KEY_CODE(KEY_TYPE_FN, 186)
#define KEY_CODE_F178          KEY_CODE(KEY_TYPE_FN, 187)
#define KEY_CODE_F179          KEY_CODE(KEY_TYPE_FN, 188)
#define KEY_CODE_F180          KEY_CODE(KEY_TYPE_FN, 189)
#define KEY_CODE_F181          KEY_CODE(KEY_TYPE_FN, 190)
#define KEY_CODE_F182          KEY_CODE(KEY_TYPE_FN, 191)
#define KEY_CODE_F183          KEY_CODE(KEY_TYPE_FN, 192)
#define KEY_CODE_F184          KEY_CODE(KEY_TYPE_FN, 193)
#define KEY_CODE_F185          KEY_CODE(KEY_TYPE_FN, 194)
#define KEY_CODE_F186          KEY_CODE(KEY_TYPE_FN, 195)
#define KEY_CODE_F187          KEY_CODE(KEY_TYPE_FN, 196)
#define KEY_CODE_F188          KEY_CODE(KEY_TYPE_FN, 197)
#define KEY_CODE_F189          KEY_CODE(KEY_TYPE_FN, 198)
#define KEY_CODE_F190          KEY_CODE(KEY_TYPE_FN, 199)
#define KEY_CODE_F191          KEY_CODE(KEY_TYPE_FN, 200)
#define KEY_CODE_F192          KEY_CODE(KEY_TYPE_FN, 201)
#define KEY_CODE_F193          KEY_CODE(KEY_TYPE_FN, 202)
#define KEY_CODE_F194          KEY_CODE(KEY_TYPE_FN, 203)
#define KEY_CODE_F195          KEY_CODE(KEY_TYPE_FN, 204)
#define KEY_CODE_F196          KEY_CODE(KEY_TYPE_FN, 205)
#define KEY_CODE_F197          KEY_CODE(KEY_TYPE_FN, 206)
#define KEY_CODE_F198          KEY_CODE(KEY_TYPE_FN, 207)
#define KEY_CODE_F199          KEY_CODE(KEY_TYPE_FN, 208)
#define KEY_CODE_F200          KEY_CODE(KEY_TYPE_FN, 209)
#define KEY_CODE_F201          KEY_CODE(KEY_TYPE_FN, 210)
#define KEY_CODE_F202          KEY_CODE(KEY_TYPE_FN, 211)
#define KEY_CODE_F203          KEY_CODE(KEY_TYPE_FN, 212)
#define KEY_CODE_F204          KEY_CODE(KEY_TYPE_FN, 213)
#define KEY_CODE_F205          KEY_CODE(KEY_TYPE_FN, 214)
#define KEY_CODE_F206          KEY_CODE(KEY_TYPE_FN, 215)
#define KEY_CODE_F207          KEY_CODE(KEY_TYPE_FN, 216)
#define KEY_CODE_F208          KEY_CODE(KEY_TYPE_FN, 217)
#define KEY_CODE_F209          KEY_CODE(KEY_TYPE_FN, 218)
#define KEY_CODE_F210          KEY_CODE(KEY_TYPE_FN, 219)
#define KEY_CODE_F211          KEY_CODE(KEY_TYPE_FN, 220)
#define KEY_CODE_F212          KEY_CODE(KEY_TYPE_FN, 221)
#define KEY_CODE_F213          KEY_CODE(KEY_TYPE_FN, 222)
#define KEY_CODE_F214          KEY_CODE(KEY_TYPE_FN, 223)
#define KEY_CODE_F215          KEY_CODE(KEY_TYPE_FN, 224)
#define KEY_CODE_F216          KEY_CODE(KEY_TYPE_FN, 225)
#define KEY_CODE_F217          KEY_CODE(KEY_TYPE_FN, 226)
#define KEY_CODE_F218          KEY_CODE(KEY_TYPE_FN, 227)
#define KEY_CODE_F219          KEY_CODE(KEY_TYPE_FN, 228)
#define KEY_CODE_F220          KEY_CODE(KEY_TYPE_FN, 229)
#define KEY_CODE_F221          KEY_CODE(KEY_TYPE_FN, 230)
#define KEY_CODE_F222          KEY_CODE(KEY_TYPE_FN, 231)
#define KEY_CODE_F223          KEY_CODE(KEY_TYPE_FN, 232)
#define KEY_CODE_F224          KEY_CODE(KEY_TYPE_FN, 233)
#define KEY_CODE_F225          KEY_CODE(KEY_TYPE_FN, 234)
#define KEY_CODE_F226          KEY_CODE(KEY_TYPE_FN, 235)
#define KEY_CODE_F227          KEY_CODE(KEY_TYPE_FN, 236)
#define KEY_CODE_F228          KEY_CODE(KEY_TYPE_FN, 237)
#define KEY_CODE_F229          KEY_CODE(KEY_TYPE_FN, 238)
#define KEY_CODE_F230          KEY_CODE(KEY_TYPE_FN, 239)
#define KEY_CODE_F231          KEY_CODE(KEY_TYPE_FN, 240)
#define KEY_CODE_F232          KEY_CODE(KEY_TYPE_FN, 241)
#define KEY_CODE_F233          KEY_CODE(KEY_TYPE_FN, 242)
#define KEY_CODE_F234          KEY_CODE(KEY_TYPE_FN, 243)
#define KEY_CODE_F235          KEY_CODE(KEY_TYPE_FN, 244)
#define KEY_CODE_F236          KEY_CODE(KEY_TYPE_FN, 245)
#define KEY_CODE_F237          KEY_CODE(KEY_TYPE_FN, 246)
#define KEY_CODE_F238          KEY_CODE(KEY_TYPE_FN, 247)
#define KEY_CODE_F239          KEY_CODE(KEY_TYPE_FN, 248)
#define KEY_CODE_F240          KEY_CODE(KEY_TYPE_FN, 249)
#define KEY_CODE_F241          KEY_CODE(KEY_TYPE_FN, 250)
#define KEY_CODE_F242          KEY_CODE(KEY_TYPE_FN, 251)
#define KEY_CODE_F243          KEY_CODE(KEY_TYPE_FN, 252)
#define KEY_CODE_F244          KEY_CODE(KEY_TYPE_FN, 253)
#define KEY_CODE_F245          KEY_CODE(KEY_TYPE_FN, 254)
#define KEY_CODE_UNDO          KEY_CODE(KEY_TYPE_FN, 255)

#define KEY_CODE_HOLE          KEY_CODE(KEY_TYPE_SPEC, 0)
#define KEY_CODE_ENTER         KEY_CODE(KEY_TYPE_SPEC, 1)
#define KEY_CODE_SH_REGS       KEY_CODE(KEY_TYPE_SPEC, 2)
#define KEY_CODE_SH_MEM        KEY_CODE(KEY_TYPE_SPEC, 3)
#define KEY_CODE_SH_STAT       KEY_CODE(KEY_TYPE_SPEC, 4)
#define KEY_CODE_BREAK         KEY_CODE(KEY_TYPE_SPEC, 5)
#define KEY_CODE_CONS          KEY_CODE(KEY_TYPE_SPEC, 6)
#define KEY_CODE_CAPS          KEY_CODE(KEY_TYPE_SPEC, 7)
#define KEY_CODE_NUM           KEY_CODE(KEY_TYPE_SPEC, 8)
#define KEY_CODE_HOLD          KEY_CODE(KEY_TYPE_SPEC, 9)
#define KEY_CODE_SCROLLFORW    KEY_CODE(KEY_TYPE_SPEC, 10)
#define KEY_CODE_SCROLLBACK    KEY_CODE(KEY_TYPE_SPEC, 11)
#define KEY_CODE_BOOT          KEY_CODE(KEY_TYPE_SPEC, 12)
#define KEY_CODE_CAPSON        KEY_CODE(KEY_TYPE_SPEC, 13)
#define KEY_CODE_COMPOSE       KEY_CODE(KEY_TYPE_SPEC, 14)
#define KEY_CODE_SAK           KEY_CODE(KEY_TYPE_SPEC, 15)
#define KEY_CODE_DECRCONSOLE   KEY_CODE(KEY_TYPE_SPEC, 16)
#define KEY_CODE_INCRCONSOLE   KEY_CODE(KEY_TYPE_SPEC, 17)
#define KEY_CODE_SPAWNCONSOLE  KEY_CODE(KEY_TYPE_SPEC, 18)
#define KEY_CODE_BARENUMLOCK   KEY_CODE(KEY_TYPE_SPEC, 19)

#define KEY_CODE_ALLOCATED     KEY_CODE(KEY_TYPE_SPEC, 126)
#define KEY_CODE_NOSUCHMAP     KEY_CODE(KEY_TYPE_SPEC, 127)

#define KEY_CODE_P0            KEY_CODE(KEY_TYPE_PAD, 0)
#define KEY_CODE_P1            KEY_CODE(KEY_TYPE_PAD, 1)
#define KEY_CODE_P2            KEY_CODE(KEY_TYPE_PAD, 2)
#define KEY_CODE_P3            KEY_CODE(KEY_TYPE_PAD, 3)
#define KEY_CODE_P4            KEY_CODE(KEY_TYPE_PAD, 4)
#define KEY_CODE_P5            KEY_CODE(KEY_TYPE_PAD, 5)
#define KEY_CODE_P6            KEY_CODE(KEY_TYPE_PAD, 6)
#define KEY_CODE_P7            KEY_CODE(KEY_TYPE_PAD, 7)
#define KEY_CODE_P8            KEY_CODE(KEY_TYPE_PAD, 8)
#define KEY_CODE_P9            KEY_CODE(KEY_TYPE_PAD, 9)
#define KEY_CODE_PPLUS         KEY_CODE(KEY_TYPE_PAD, 10)
#define KEY_CODE_PMINUS        KEY_CODE(KEY_TYPE_PAD, 11)
#define KEY_CODE_PSTAR         KEY_CODE(KEY_TYPE_PAD, 12)
#define KEY_CODE_PSLASH        KEY_CODE(KEY_TYPE_PAD, 13)
#define KEY_CODE_PENTER        KEY_CODE(KEY_TYPE_PAD, 14)
#define KEY_CODE_PCOMMA        KEY_CODE(KEY_TYPE_PAD, 15)
#define KEY_CODE_PDOT          KEY_CODE(KEY_TYPE_PAD, 16)
#define KEY_CODE_PPLUSMINUS    KEY_CODE(KEY_TYPE_PAD, 17)
#define KEY_CODE_PPARENL       KEY_CODE(KEY_TYPE_PAD, 18)
#define KEY_CODE_PPARENR       KEY_CODE(KEY_TYPE_PAD, 19)

#define KEY_CODE_DGRAVE        KEY_CODE(KEY_TYPE_DEAD, 0)
#define KEY_CODE_DACUTE        KEY_CODE(KEY_TYPE_DEAD, 1)
#define KEY_CODE_DCIRCM        KEY_CODE(KEY_TYPE_DEAD, 2)
#define KEY_CODE_DTILDE        KEY_CODE(KEY_TYPE_DEAD, 3)
#define KEY_CODE_DDIERE        KEY_CODE(KEY_TYPE_DEAD, 4)
#define KEY_CODE_DCEDIL        KEY_CODE(KEY_TYPE_DEAD, 5)

#define KEY_CODE_DOWN          KEY_CODE(KEY_TYPE_CUR, 0)
#define KEY_CODE_LEFT          KEY_CODE(KEY_TYPE_CUR, 1)
#define KEY_CODE_RIGHT         KEY_CODE(KEY_TYPE_CUR, 2)
#define KEY_CODE_UP            KEY_CODE(KEY_TYPE_CUR, 3)

#define KEY_CODE_SHIFT         KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_SHIFT)
#define KEY_CODE_CTRL          KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_CTRL)
#define KEY_CODE_ALT           KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_ALT)
#define KEY_CODE_ALTGR         KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_ALTGR)
#define KEY_CODE_SHIFTL        KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_SHIFTL)
#define KEY_CODE_SHIFTR        KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_SHIFTR)
#define KEY_CODE_CTRLL         KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_CTRLL)
#define KEY_CODE_CTRLR         KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_CTRLR)
#define KEY_CODE_CAPSSHIFT     KEY_CODE(KEY_TYPE_SHIFT, KEY_MODIFIER_CAPSSHIFT)

#define KEY_CODE_ASC0          KEY_CODE(KEY_TYPE_ASCII, 0)
#define KEY_CODE_ASC1          KEY_CODE(KEY_TYPE_ASCII, 1)
#define KEY_CODE_ASC2          KEY_CODE(KEY_TYPE_ASCII, 2)
#define KEY_CODE_ASC3          KEY_CODE(KEY_TYPE_ASCII, 3)
#define KEY_CODE_ASC4          KEY_CODE(KEY_TYPE_ASCII, 4)
#define KEY_CODE_ASC5          KEY_CODE(KEY_TYPE_ASCII, 5)
#define KEY_CODE_ASC6          KEY_CODE(KEY_TYPE_ASCII, 6)
#define KEY_CODE_ASC7          KEY_CODE(KEY_TYPE_ASCII, 7)
#define KEY_CODE_ASC8          KEY_CODE(KEY_TYPE_ASCII, 8)
#define KEY_CODE_ASC9          KEY_CODE(KEY_TYPE_ASCII, 9)
#define KEY_CODE_HEX0          KEY_CODE(KEY_TYPE_ASCII, 10)
#define KEY_CODE_HEX1          KEY_CODE(KEY_TYPE_ASCII, 11)
#define KEY_CODE_HEX2          KEY_CODE(KEY_TYPE_ASCII, 12)
#define KEY_CODE_HEX3          KEY_CODE(KEY_TYPE_ASCII, 13)
#define KEY_CODE_HEX4          KEY_CODE(KEY_TYPE_ASCII, 14)
#define KEY_CODE_HEX5          KEY_CODE(KEY_TYPE_ASCII, 15)
#define KEY_CODE_HEX6          KEY_CODE(KEY_TYPE_ASCII, 16)
#define KEY_CODE_HEX7          KEY_CODE(KEY_TYPE_ASCII, 17)
#define KEY_CODE_HEX8          KEY_CODE(KEY_TYPE_ASCII, 18)
#define KEY_CODE_HEX9          KEY_CODE(KEY_TYPE_ASCII, 19)
#define KEY_CODE_HEXa          KEY_CODE(KEY_TYPE_ASCII, 20)
#define KEY_CODE_HEXb          KEY_CODE(KEY_TYPE_ASCII, 21)
#define KEY_CODE_HEXc          KEY_CODE(KEY_TYPE_ASCII, 22)
#define KEY_CODE_HEXd          KEY_CODE(KEY_TYPE_ASCII, 23)
#define KEY_CODE_HEXe          KEY_CODE(KEY_TYPE_ASCII, 24)
#define KEY_CODE_HEXf          KEY_CODE(KEY_TYPE_ASCII, 25)

#define KEY_CODE_SHIFTLOCK     KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_SHIFT)
#define KEY_CODE_CTRLLOCK      KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_CTRL)
#define KEY_CODE_ALTLOCK       KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_ALT)
#define KEY_CODE_ALTGRLOCK     KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_ALTGR)
#define KEY_CODE_SHIFTLLOCK    KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_SHIFTL)
#define KEY_CODE_SHIFTRLOCK    KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_SHIFTR)
#define KEY_CODE_CTRLLLOCK     KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_CTRLL)
#define KEY_CODE_CTRLRLOCK     KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_CTRLR)
#define KEY_CODE_CAPSSHIFTLOCK KEY_CODE(KEY_TYPE_LOCK, KEY_MODIFIER_CAPSSHIFT)

#define KEY_CODE_SHIFT_SLOCK      KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_SHIFT)
#define KEY_CODE_CTRL_SLOCK       KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_CTRL)
#define KEY_CODE_ALT_SLOCK		  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_ALT)
#define KEY_CODE_ALTGR_SLOCK	  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_ALTGR)
#define KEY_CODE_SHIFTL_SLOCK	  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_SHIFTL)
#define KEY_CODE_SHIFTR_SLOCK	  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_SHIFTR)
#define KEY_CODE_CTRLL_SLOCK	  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_CTRLL)
#define KEY_CODE_CTRLR_SLOCK	  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_CTRLR)
#define KEY_CODE_CAPSSHIFT_SLOCK  KEY_CODE(KEY_TYPE_SLOCK, KEY_MODIFIER_CAPSSHIFT)

#define KEY_CODE_BRL_BLANK     KEY_CODE(KEY_TYPE_BRL, 0)
#define KEY_CODE_BRL_DOT1      KEY_CODE(KEY_TYPE_BRL, 1)
#define KEY_CODE_BRL_DOT2      KEY_CODE(KEY_TYPE_BRL, 2)
#define KEY_CODE_BRL_DOT3      KEY_CODE(KEY_TYPE_BRL, 3)
#define KEY_CODE_BRL_DOT4      KEY_CODE(KEY_TYPE_BRL, 4)
#define KEY_CODE_BRL_DOT5      KEY_CODE(KEY_TYPE_BRL, 5)
#define KEY_CODE_BRL_DOT6      KEY_CODE(KEY_TYPE_BRL, 6)
#define KEY_CODE_BRL_DOT7      KEY_CODE(KEY_TYPE_BRL, 7)
#define KEY_CODE_BRL_DOT8      KEY_CODE(KEY_TYPE_BRL, 8)
#define KEY_CODE_BRL_DOT9      KEY_CODE(KEY_TYPE_BRL, 9)
#define KEY_CODE_BRL_DOT10     KEY_CODE(KEY_TYPE_BRL, 10)

/**
 * \addtogroup Keyboard
 * @{
 */

struct keyModifier
{
	uint8_t shift		: 1; /**< is the shift key pressed? */
	uint8_t shiftLocked : 1; /**< is capslock enabled?		*/
	uint8_t ctrl		: 1; /**< is the ctrl key pressed?	*/
	uint8_t alt			: 1; /**< is the alt key pressed?	*/
	uint8_t altgr		: 1; /**< is the altgr key pressed? */
};

struct keyEvent
{
	bool				pressed;	/** was key pressed or released? */
	struct keyModifier	modifiers;	/** key modifier map (shift, ctrl, ...) */
	uint16_t			keyCode;	/** the key code of pressed/ released key */
};

/**
 * @anchor keyboardLED
 * @name Keyboard LEDs
 * @{
 */
#define KEYBOARD_LED_SCROLLOCK	1
#define KEYBOARD_LED_NUMLOCK	2
#define KEYBOARD_LED_CAPSLOCK	4
/**
 * @}
 */

#ifdef __KERNEL__

	#include <process/object.h>

	#define KEYBOARD_BUFFER_PORT		0x60
	#define KEYBOARD_STATUS_PORT		0x64

	/* see  http://www.lowlevel.eu/wiki/Keyboard_Controller#KBC-Befehle */
	/**
	 * @name Keyboard Controller commands
	 * @{
	 */
	#define KEYBOARD_KDC_TEST_KEYBOARD		0xAA
	#define KEYBOARD_KDC_TEST_CONNECTION	0xAB
	#define KEYBOARD_KDC_DISABLE			0xAD
	#define KEYBOARD_KDC_ENABLE				0xAE
	#define KEYBOARD_KDC_READ_INPUT			0xC0
	#define KEYBOARD_KDC_READ_OUTPUT		0xD0
	#define KEYBOARD_KDC_WRITE_OUTPUT		0xD1
	/**
	 * @}
	 *
	 * @name Keyboard commands
	 * @{
	 */
	#define KEYBOARD_CMD_LED		0xED
	#define KEYBOARD_CMD_TEST		0xEE
	#define KEYBOARD_CMD_SCANCODES	0xF0
	#define KEYBOARD_CMD_IDENTIFY	0xF2
	#define KEYBOARD_CMD_REPEAT		0xF3
	#define KEYBOARD_CMD_ENABLE		0xF4
	#define KEYBOARD_CMD_DISABLE	0xF5
	#define KEYBOARD_CMD_DEFAULT	0xF6
	#define KEYBOARD_CMD_RESET_TEST	0xFF
	/**
	 * @}
	 */

	struct keyMapInfo
	{
		uint16_t *plain_map;
		uint16_t *shift_map;
		uint16_t *altgr_map;
		uint16_t *alt_map;
		uint16_t *ctrl_map;
		uint16_t *shift_ctrl_map;
		uint16_t *altgr_ctrl_map;
		uint16_t *shift_alt_map;
		uint16_t *altgr_alt_map;
		uint16_t *ctrl_alt_map;
	};

	struct keyMap
	{
		char code[2];
		struct keyMapInfo keyMaps;
	};

	void keyboardInit(struct object *obj);
	void keyboardSend(uint8_t cmd);
	void keyboardSetLEDFlags(uint32_t flags);
	uint32_t keyboardGetLEDFlags();

#endif
/**
 * @}
 */

#endif /* _H_KEYBOARD_ */