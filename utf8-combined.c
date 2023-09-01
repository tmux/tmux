/* $OpenBSD$ */

/*
 * Copyright (c) 2023 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "tmux.h"

static const struct {
	wchar_t first;
	wchar_t second;
} utf8_combined_table[] = {
	{ 0x1F1E6, 0x1F1E8 }, /* flag: Ascension Island */
	{ 0x1F1E6, 0x1F1E9 }, /* flag: Andorra */
	{ 0x1F1E6, 0x1F1EA }, /* flag: United Arab Emirates */
	{ 0x1F1E6, 0x1F1EB }, /* flag: Afghanistan */
	{ 0x1F1E6, 0x1F1EC }, /* flag: Antigua & Barbuda */
	{ 0x1F1E6, 0x1F1EE }, /* flag: Anguilla */
	{ 0x1F1E6, 0x1F1F1 }, /* flag: Albania */
	{ 0x1F1E6, 0x1F1F2 }, /* flag: Armenia */
	{ 0x1F1E6, 0x1F1F4 }, /* flag: Angola */
	{ 0x1F1E6, 0x1F1F6 }, /* flag: Antarctica */
	{ 0x1F1E6, 0x1F1F7 }, /* flag: Argentina */
	{ 0x1F1E6, 0x1F1F8 }, /* flag: American Samoa */
	{ 0x1F1E6, 0x1F1F9 }, /* flag: Austria */
	{ 0x1F1E6, 0x1F1FA }, /* flag: Australia */
	{ 0x1F1E6, 0x1F1FC }, /* flag: Aruba */
	{ 0x1F1E6, 0x1F1FD }, /* flag: Aland Islands */
	{ 0x1F1E6, 0x1F1FF }, /* flag: Azerbaijan */
	{ 0x1F1E7, 0x1F1E6 }, /* flag: Bosnia & Herzegovina */
	{ 0x1F1E7, 0x1F1E7 }, /* flag: Barbados */
	{ 0x1F1E7, 0x1F1E9 }, /* flag: Bangladesh */
	{ 0x1F1E7, 0x1F1EA }, /* flag: Belgium */
	{ 0x1F1E7, 0x1F1EB }, /* flag: Burkina Faso */
	{ 0x1F1E7, 0x1F1EC }, /* flag: Bulgaria */
	{ 0x1F1E7, 0x1F1ED }, /* flag: Bahrain */
	{ 0x1F1E7, 0x1F1EE }, /* flag: Burundi */
	{ 0x1F1E7, 0x1F1EF }, /* flag: Benin */
	{ 0x1F1E7, 0x1F1F1 }, /* flag: St. Barthelemy */
	{ 0x1F1E7, 0x1F1F2 }, /* flag: Bermuda */
	{ 0x1F1E7, 0x1F1F3 }, /* flag: Brunei */
	{ 0x1F1E7, 0x1F1F4 }, /* flag: Bolivia */
	{ 0x1F1E7, 0x1F1F6 }, /* flag: Caribbean Netherlands */
	{ 0x1F1E7, 0x1F1F7 }, /* flag: Brazil */
	{ 0x1F1E7, 0x1F1F8 }, /* flag: Bahamas */
	{ 0x1F1E7, 0x1F1F9 }, /* flag: Bhutan */
	{ 0x1F1E7, 0x1F1FB }, /* flag: Bouvet Island */
	{ 0x1F1E7, 0x1F1FC }, /* flag: Botswana */
	{ 0x1F1E7, 0x1F1FE }, /* flag: Belarus */
	{ 0x1F1E7, 0x1F1FF }, /* flag: Belize */
	{ 0x1F1E8, 0x1F1E6 }, /* flag: Canada */
	{ 0x1F1E8, 0x1F1E8 }, /* flag: Cocos (Keeling) Islands */
	{ 0x1F1E8, 0x1F1E9 }, /* flag: Congo - Kinshasa */
	{ 0x1F1E8, 0x1F1EB }, /* flag: Central African Republic */
	{ 0x1F1E8, 0x1F1EC }, /* flag: Congo - Brazzaville */
	{ 0x1F1E8, 0x1F1ED }, /* flag: Switzerland */
	{ 0x1F1E8, 0x1F1EE }, /* flag: Cote d'Ivoire */
	{ 0x1F1E8, 0x1F1F0 }, /* flag: Cook Islands */
	{ 0x1F1E8, 0x1F1F1 }, /* flag: Chile */
	{ 0x1F1E8, 0x1F1F2 }, /* flag: Cameroon */
	{ 0x1F1E8, 0x1F1F3 }, /* flag: China */
	{ 0x1F1E8, 0x1F1F4 }, /* flag: Colombia */
	{ 0x1F1E8, 0x1F1F5 }, /* flag: Clipperton Island */
	{ 0x1F1E8, 0x1F1F7 }, /* flag: Costa Rica */
	{ 0x1F1E8, 0x1F1FA }, /* flag: Cuba */
	{ 0x1F1E8, 0x1F1FB }, /* flag: Cape Verde */
	{ 0x1F1E8, 0x1F1FC }, /* flag: Curacao */
	{ 0x1F1E8, 0x1F1FD }, /* flag: Christmas Island */
	{ 0x1F1E8, 0x1F1FE }, /* flag: Cyprus */
	{ 0x1F1E8, 0x1F1FF }, /* flag: Czechia */
	{ 0x1F1E9, 0x1F1EA }, /* flag: Germany */
	{ 0x1F1E9, 0x1F1EC }, /* flag: Diego Garcia */
	{ 0x1F1E9, 0x1F1EF }, /* flag: Djibouti */
	{ 0x1F1E9, 0x1F1F0 }, /* flag: Denmark */
	{ 0x1F1E9, 0x1F1F2 }, /* flag: Dominica */
	{ 0x1F1E9, 0x1F1F4 }, /* flag: Dominican Republic */
	{ 0x1F1E9, 0x1F1FF }, /* flag: Algeria */
	{ 0x1F1EA, 0x1F1E6 }, /* flag: Ceuta & Melilla */
	{ 0x1F1EA, 0x1F1E8 }, /* flag: Ecuador */
	{ 0x1F1EA, 0x1F1EA }, /* flag: Estonia */
	{ 0x1F1EA, 0x1F1EC }, /* flag: Egypt */
	{ 0x1F1EA, 0x1F1ED }, /* flag: Western Sahara */
	{ 0x1F1EA, 0x1F1F7 }, /* flag: Eritrea */
	{ 0x1F1EA, 0x1F1F8 }, /* flag: Spain */
	{ 0x1F1EA, 0x1F1F9 }, /* flag: Ethiopia */
	{ 0x1F1EA, 0x1F1FA }, /* flag: European Union */
	{ 0x1F1EB, 0x1F1EE }, /* flag: Finland */
	{ 0x1F1EB, 0x1F1EF }, /* flag: Fiji */
	{ 0x1F1EB, 0x1F1F0 }, /* flag: Falkland Islands */
	{ 0x1F1EB, 0x1F1F2 }, /* flag: Micronesia */
	{ 0x1F1EB, 0x1F1F4 }, /* flag: Faroe Islands */
	{ 0x1F1EB, 0x1F1F7 }, /* flag: France */
	{ 0x1F1EC, 0x1F1E6 }, /* flag: Gabon */
	{ 0x1F1EC, 0x1F1E7 }, /* flag: United Kingdom */
	{ 0x1F1EC, 0x1F1E9 }, /* flag: Grenada */
	{ 0x1F1EC, 0x1F1EA }, /* flag: Georgia */
	{ 0x1F1EC, 0x1F1EB }, /* flag: French Guiana */
	{ 0x1F1EC, 0x1F1EC }, /* flag: Guernsey */
	{ 0x1F1EC, 0x1F1ED }, /* flag: Ghana */
	{ 0x1F1EC, 0x1F1EE }, /* flag: Gibraltar */
	{ 0x1F1EC, 0x1F1F1 }, /* flag: Greenland */
	{ 0x1F1EC, 0x1F1F2 }, /* flag: Gambia */
	{ 0x1F1EC, 0x1F1F3 }, /* flag: Guinea */
	{ 0x1F1EC, 0x1F1F5 }, /* flag: Guadeloupe */
	{ 0x1F1EC, 0x1F1F6 }, /* flag: Equatorial Guinea */
	{ 0x1F1EC, 0x1F1F7 }, /* flag: Greece */
	{ 0x1F1EC, 0x1F1F8 }, /* flag: South Georgia & South Sandwich Islands */
	{ 0x1F1EC, 0x1F1F9 }, /* flag: Guatemala */
	{ 0x1F1EC, 0x1F1FA }, /* flag: Guam */
	{ 0x1F1EC, 0x1F1FC }, /* flag: Guinea-Bissau */
	{ 0x1F1EC, 0x1F1FE }, /* flag: Guyana */
	{ 0x1F1ED, 0x1F1F0 }, /* flag: Hong Kong SAR China */
	{ 0x1F1ED, 0x1F1F2 }, /* flag: Heard & McDonald Islands */
	{ 0x1F1ED, 0x1F1F3 }, /* flag: Honduras */
	{ 0x1F1ED, 0x1F1F7 }, /* flag: Croatia */
	{ 0x1F1ED, 0x1F1F9 }, /* flag: Haiti */
	{ 0x1F1ED, 0x1F1FA }, /* flag: Hungary */
	{ 0x1F1EE, 0x1F1E8 }, /* flag: Canary Islands */
	{ 0x1F1EE, 0x1F1E9 }, /* flag: Indonesia */
	{ 0x1F1EE, 0x1F1EA }, /* flag: Ireland */
	{ 0x1F1EE, 0x1F1F1 }, /* flag: Israel */
	{ 0x1F1EE, 0x1F1F2 }, /* flag: Isle of Man */
	{ 0x1F1EE, 0x1F1F3 }, /* flag: India */
	{ 0x1F1EE, 0x1F1F4 }, /* flag: British Indian Ocean Territory */
	{ 0x1F1EE, 0x1F1F6 }, /* flag: Iraq */
	{ 0x1F1EE, 0x1F1F7 }, /* flag: Iran */
	{ 0x1F1EE, 0x1F1F8 }, /* flag: Iceland */
	{ 0x1F1EE, 0x1F1F9 }, /* flag: Italy */
	{ 0x1F1EF, 0x1F1EA }, /* flag: Jersey */
	{ 0x1F1EF, 0x1F1F2 }, /* flag: Jamaica */
	{ 0x1F1EF, 0x1F1F4 }, /* flag: Jordan */
	{ 0x1F1EF, 0x1F1F5 }, /* flag: Japan */
	{ 0x1F1F0, 0x1F1EA }, /* flag: Kenya */
	{ 0x1F1F0, 0x1F1EC }, /* flag: Kyrgyzstan */
	{ 0x1F1F0, 0x1F1ED }, /* flag: Cambodia */
	{ 0x1F1F0, 0x1F1EE }, /* flag: Kiribati */
	{ 0x1F1F0, 0x1F1F2 }, /* flag: Comoros */
	{ 0x1F1F0, 0x1F1F3 }, /* flag: St. Kitts & Nevis */
	{ 0x1F1F0, 0x1F1F5 }, /* flag: North Korea */
	{ 0x1F1F0, 0x1F1F7 }, /* flag: South Korea */
	{ 0x1F1F0, 0x1F1FC }, /* flag: Kuwait */
	{ 0x1F1F0, 0x1F1FE }, /* flag: Cayman Islands */
	{ 0x1F1F0, 0x1F1FF }, /* flag: Kazakhstan */
	{ 0x1F1F1, 0x1F1E6 }, /* flag: Laos */
	{ 0x1F1F1, 0x1F1E7 }, /* flag: Lebanon */
	{ 0x1F1F1, 0x1F1E8 }, /* flag: St. Lucia */
	{ 0x1F1F1, 0x1F1EE }, /* flag: Liechtenstein */
	{ 0x1F1F1, 0x1F1F0 }, /* flag: Sri Lanka */
	{ 0x1F1F1, 0x1F1F7 }, /* flag: Liberia */
	{ 0x1F1F1, 0x1F1F8 }, /* flag: Lesotho */
	{ 0x1F1F1, 0x1F1F9 }, /* flag: Lithuania */
	{ 0x1F1F1, 0x1F1FA }, /* flag: Luxembourg */
	{ 0x1F1F1, 0x1F1FB }, /* flag: Latvia */
	{ 0x1F1F1, 0x1F1FE }, /* flag: Libya */
	{ 0x1F1F2, 0x1F1E6 }, /* flag: Morocco */
	{ 0x1F1F2, 0x1F1E8 }, /* flag: Monaco */
	{ 0x1F1F2, 0x1F1E9 }, /* flag: Moldova */
	{ 0x1F1F2, 0x1F1EA }, /* flag: Montenegro */
	{ 0x1F1F2, 0x1F1EB }, /* flag: St. Martin */
	{ 0x1F1F2, 0x1F1EC }, /* flag: Madagascar */
	{ 0x1F1F2, 0x1F1ED }, /* flag: Marshall Islands */
	{ 0x1F1F2, 0x1F1F0 }, /* flag: North Macedonia */
	{ 0x1F1F2, 0x1F1F1 }, /* flag: Mali */
	{ 0x1F1F2, 0x1F1F2 }, /* flag: Myanmar (Burma */
	{ 0x1F1F2, 0x1F1F3 }, /* flag: Mongolia */
	{ 0x1F1F2, 0x1F1F4 }, /* flag: Macao SAR China */
	{ 0x1F1F2, 0x1F1F5 }, /* flag: Northern Mariana Islands */
	{ 0x1F1F2, 0x1F1F6 }, /* flag: Martinique */
	{ 0x1F1F2, 0x1F1F7 }, /* flag: Mauritania */
	{ 0x1F1F2, 0x1F1F8 }, /* flag: Montserrat */
	{ 0x1F1F2, 0x1F1F9 }, /* flag: Malta */
	{ 0x1F1F2, 0x1F1FA }, /* flag: Mauritius */
	{ 0x1F1F2, 0x1F1FB }, /* flag: Maldives */
	{ 0x1F1F2, 0x1F1FC }, /* flag: Malawi */
	{ 0x1F1F2, 0x1F1FD }, /* flag: Mexico */
	{ 0x1F1F2, 0x1F1FE }, /* flag: Malaysia */
	{ 0x1F1F2, 0x1F1FF }, /* flag: Mozambique */
	{ 0x1F1F3, 0x1F1E6 }, /* flag: Namibia */
	{ 0x1F1F3, 0x1F1E8 }, /* flag: New Caledonia */
	{ 0x1F1F3, 0x1F1EA }, /* flag: Niger */
	{ 0x1F1F3, 0x1F1EB }, /* flag: Norfolk Island */
	{ 0x1F1F3, 0x1F1EC }, /* flag: Nigeria */
	{ 0x1F1F3, 0x1F1EE }, /* flag: Nicaragua */
	{ 0x1F1F3, 0x1F1F1 }, /* flag: Netherlands */
	{ 0x1F1F3, 0x1F1F4 }, /* flag: Norway */
	{ 0x1F1F3, 0x1F1F5 }, /* flag: Nepal */
	{ 0x1F1F3, 0x1F1F7 }, /* flag: Nauru */
	{ 0x1F1F3, 0x1F1FA }, /* flag: Niue */
	{ 0x1F1F3, 0x1F1FF }, /* flag: New Zealand */
	{ 0x1F1F4, 0x1F1F2 }, /* flag: Oman */
	{ 0x1F1F5, 0x1F1E6 }, /* flag: Panama */
	{ 0x1F1F5, 0x1F1EA }, /* flag: Peru */
	{ 0x1F1F5, 0x1F1EB }, /* flag: French Polynesia */
	{ 0x1F1F5, 0x1F1EC }, /* flag: Papua New Guinea */
	{ 0x1F1F5, 0x1F1ED }, /* flag: Philippines */
	{ 0x1F1F5, 0x1F1F0 }, /* flag: Pakistan */
	{ 0x1F1F5, 0x1F1F1 }, /* flag: Poland */
	{ 0x1F1F5, 0x1F1F2 }, /* flag: St. Pierre & Miquelon */
	{ 0x1F1F5, 0x1F1F3 }, /* flag: Pitcairn Islands */
	{ 0x1F1F5, 0x1F1F7 }, /* flag: Puerto Rico */
	{ 0x1F1F5, 0x1F1F8 }, /* flag: Palestinian Territories */
	{ 0x1F1F5, 0x1F1F9 }, /* flag: Portugal */
	{ 0x1F1F5, 0x1F1FC }, /* flag: Palau */
	{ 0x1F1F5, 0x1F1FE }, /* flag: Paraguay */
	{ 0x1F1F6, 0x1F1E6 }, /* flag: Qatar */
	{ 0x1F1F7, 0x1F1EA }, /* flag: Reunion */
	{ 0x1F1F7, 0x1F1F4 }, /* flag: Romania */
	{ 0x1F1F7, 0x1F1F8 }, /* flag: Serbia */
	{ 0x1F1F7, 0x1F1FA }, /* flag: Russia */
	{ 0x1F1F7, 0x1F1FC }, /* flag: Rwanda */
	{ 0x1F1F8, 0x1F1E6 }, /* flag: Saudi Arabia */
	{ 0x1F1F8, 0x1F1E7 }, /* flag: Solomon Islands */
	{ 0x1F1F8, 0x1F1E8 }, /* flag: Seychelles */
	{ 0x1F1F8, 0x1F1E9 }, /* flag: Sudan */
	{ 0x1F1F8, 0x1F1EA }, /* flag: Sweden */
	{ 0x1F1F8, 0x1F1EC }, /* flag: Singapore */
	{ 0x1F1F8, 0x1F1ED }, /* flag: St. Helena */
	{ 0x1F1F8, 0x1F1EE }, /* flag: Slovenia */
	{ 0x1F1F8, 0x1F1EF }, /* flag: Svalbard & Jan Mayen */
	{ 0x1F1F8, 0x1F1F0 }, /* flag: Slovakia */
	{ 0x1F1F8, 0x1F1F1 }, /* flag: Sierra Leone */
	{ 0x1F1F8, 0x1F1F2 }, /* flag: San Marino */
	{ 0x1F1F8, 0x1F1F3 }, /* flag: Senegal */
	{ 0x1F1F8, 0x1F1F4 }, /* flag: Somalia */
	{ 0x1F1F8, 0x1F1F7 }, /* flag: Suriname */
	{ 0x1F1F8, 0x1F1F8 }, /* flag: South Sudan */
	{ 0x1F1F8, 0x1F1F9 }, /* flag: Sao Tome & Principe */
	{ 0x1F1F8, 0x1F1FB }, /* flag: El Salvador */
	{ 0x1F1F8, 0x1F1FD }, /* flag: Sint Maarten */
	{ 0x1F1F8, 0x1F1FE }, /* flag: Syria */
	{ 0x1F1F8, 0x1F1FF }, /* flag: Eswatini */
	{ 0x1F1F9, 0x1F1E6 }, /* flag: Tristan da Cunha */
	{ 0x1F1F9, 0x1F1E8 }, /* flag: Turks & Caicos Islands */
	{ 0x1F1F9, 0x1F1E9 }, /* flag: Chad */
	{ 0x1F1F9, 0x1F1EB }, /* flag: French Southern Territories */
	{ 0x1F1F9, 0x1F1EC }, /* flag: Togo */
	{ 0x1F1F9, 0x1F1ED }, /* flag: Thailand */
	{ 0x1F1F9, 0x1F1EF }, /* flag: Tajikistan */
	{ 0x1F1F9, 0x1F1F0 }, /* flag: Tokelau */
	{ 0x1F1F9, 0x1F1F1 }, /* flag: Timor-Leste */
	{ 0x1F1F9, 0x1F1F2 }, /* flag: Turkmenistan */
	{ 0x1F1F9, 0x1F1F3 }, /* flag: Tunisia */
	{ 0x1F1F9, 0x1F1F4 }, /* flag: Tonga */
	{ 0x1F1F9, 0x1F1F7 }, /* flag: Turkey */
	{ 0x1F1F9, 0x1F1F9 }, /* flag: Trinidad & Tobago */
	{ 0x1F1F9, 0x1F1FB }, /* flag: Tuvalu */
	{ 0x1F1F9, 0x1F1FC }, /* flag: Taiwan */
	{ 0x1F1F9, 0x1F1FF }, /* flag: Tanzania */
	{ 0x1F1FA, 0x1F1E6 }, /* flag: Ukraine */
	{ 0x1F1FA, 0x1F1EC }, /* flag: Uganda */
	{ 0x1F1FA, 0x1F1F2 }, /* flag: U.S. Outlying Islands */
	{ 0x1F1FA, 0x1F1F3 }, /* flag: United Nations */
	{ 0x1F1FA, 0x1F1F8 }, /* flag: United States */
	{ 0x1F1FA, 0x1F1FE }, /* flag: Uruguay */
	{ 0x1F1FA, 0x1F1FF }, /* flag: Uzbekistan */
	{ 0x1F1FB, 0x1F1E6 }, /* flag: Vatican City */
	{ 0x1F1FB, 0x1F1E8 }, /* flag: St. Vincent & Grenadines */
	{ 0x1F1FB, 0x1F1EA }, /* flag: Venezuela */
	{ 0x1F1FB, 0x1F1EC }, /* flag: British Virgin Islands */
	{ 0x1F1FB, 0x1F1EE }, /* flag: U.S. Virgin Islands */
	{ 0x1F1FB, 0x1F1F3 }, /* flag: Vietnam */
	{ 0x1F1FB, 0x1F1FA }, /* flag: Vanuatu */
	{ 0x1F1FC, 0x1F1EB }, /* flag: Wallis & Futuna */
	{ 0x1F1FC, 0x1F1F8 }, /* flag: Samoa */
	{ 0x1F1FD, 0x1F1F0 }, /* flag: Kosovo */
	{ 0x1F1FE, 0x1F1EA }, /* flag: Yemen */
	{ 0x1F1FE, 0x1F1F9 }, /* flag: Mayotte */
	{ 0x1F1FF, 0x1F1E6 }, /* flag: South Africa */
	{ 0x1F1FF, 0x1F1F2 }, /* flag: Zambia */
	{ 0x1F1FF, 0x1F1FC }, /* flag: Zimbabwe */
	{ 0x0261D, 0x1F3FB }, /* index pointing up: light skin tone */
	{ 0x0261D, 0x1F3FC }, /* index pointing up: medium-light skin tone */
	{ 0x0261D, 0x1F3FD }, /* index pointing up: medium skin tone */
	{ 0x0261D, 0x1F3FE }, /* index pointing up: medium-dark skin tone */
	{ 0x0261D, 0x1F3FF }, /* index pointing up: dark skin tone */
	{ 0x026F9, 0x1F3FB }, /* person bouncing ball: light skin tone */
	{ 0x026F9, 0x1F3FC }, /* person bouncing ball: medium-light skin tone */
	{ 0x026F9, 0x1F3FD }, /* person bouncing ball: medium skin tone */
	{ 0x026F9, 0x1F3FE }, /* person bouncing ball: medium-dark skin tone */
	{ 0x026F9, 0x1F3FF }, /* person bouncing ball: dark skin tone */
	{ 0x0270A, 0x1F3FB }, /* raised fist: light skin tone */
	{ 0x0270A, 0x1F3FC }, /* raised fist: medium-light skin tone */
	{ 0x0270A, 0x1F3FD }, /* raised fist: medium skin tone */
	{ 0x0270A, 0x1F3FE }, /* raised fist: medium-dark skin tone */
	{ 0x0270A, 0x1F3FF }, /* raised fist: dark skin tone */
	{ 0x0270B, 0x1F3FB }, /* raised hand: light skin tone */
	{ 0x0270B, 0x1F3FC }, /* raised hand: medium-light skin tone */
	{ 0x0270B, 0x1F3FD }, /* raised hand: medium skin tone */
	{ 0x0270B, 0x1F3FE }, /* raised hand: medium-dark skin tone */
	{ 0x0270B, 0x1F3FF }, /* raised hand: dark skin tone */
	{ 0x0270C, 0x1F3FB }, /* victory hand: light skin tone */
	{ 0x0270C, 0x1F3FC }, /* victory hand: medium-light skin tone */
	{ 0x0270C, 0x1F3FD }, /* victory hand: medium skin tone */
	{ 0x0270C, 0x1F3FE }, /* victory hand: medium-dark skin tone */
	{ 0x0270C, 0x1F3FF }, /* victory hand: dark skin tone */
	{ 0x0270D, 0x1F3FB }, /* writing hand: light skin tone */
	{ 0x0270D, 0x1F3FC }, /* writing hand: medium-light skin tone */
	{ 0x0270D, 0x1F3FD }, /* writing hand: medium skin tone */
	{ 0x0270D, 0x1F3FE }, /* writing hand: medium-dark skin tone */
	{ 0x0270D, 0x1F3FF }, /* writing hand: dark skin tone */
	{ 0x1F385, 0x1F3FB }, /* Santa Claus: light skin tone */
	{ 0x1F385, 0x1F3FC }, /* Santa Claus: medium-light skin tone */
	{ 0x1F385, 0x1F3FD }, /* Santa Claus: medium skin tone */
	{ 0x1F385, 0x1F3FE }, /* Santa Claus: medium-dark skin tone */
	{ 0x1F385, 0x1F3FF }, /* Santa Claus: dark skin tone */
	{ 0x1F3C2, 0x1F3FB }, /* snowboarder: light skin tone */
	{ 0x1F3C2, 0x1F3FC }, /* snowboarder: medium-light skin tone */
	{ 0x1F3C2, 0x1F3FD }, /* snowboarder: medium skin tone */
	{ 0x1F3C2, 0x1F3FE }, /* snowboarder: medium-dark skin tone */
	{ 0x1F3C2, 0x1F3FF }, /* snowboarder: dark skin tone */
	{ 0x1F3C3, 0x1F3FB }, /* person running: light skin tone */
	{ 0x1F3C3, 0x1F3FC }, /* person running: medium-light skin tone */
	{ 0x1F3C3, 0x1F3FD }, /* person running: medium skin tone */
	{ 0x1F3C3, 0x1F3FE }, /* person running: medium-dark skin tone */
	{ 0x1F3C3, 0x1F3FF }, /* person running: dark skin tone */
	{ 0x1F3C4, 0x1F3FB }, /* person surfing: light skin tone */
	{ 0x1F3C4, 0x1F3FC }, /* person surfing: medium-light skin tone */
	{ 0x1F3C4, 0x1F3FD }, /* person surfing: medium skin tone */
	{ 0x1F3C4, 0x1F3FE }, /* person surfing: medium-dark skin tone */
	{ 0x1F3C4, 0x1F3FF }, /* person surfing: dark skin tone */
	{ 0x1F3C7, 0x1F3FB }, /* horse racing: light skin tone */
	{ 0x1F3C7, 0x1F3FC }, /* horse racing: medium-light skin tone */
	{ 0x1F3C7, 0x1F3FD }, /* horse racing: medium skin tone */
	{ 0x1F3C7, 0x1F3FE }, /* horse racing: medium-dark skin tone */
	{ 0x1F3C7, 0x1F3FF }, /* horse racing: dark skin tone */
	{ 0x1F3CA, 0x1F3FB }, /* person swimming: light skin tone */
	{ 0x1F3CA, 0x1F3FC }, /* person swimming: medium-light skin tone */
	{ 0x1F3CA, 0x1F3FD }, /* person swimming: medium skin tone */
	{ 0x1F3CA, 0x1F3FE }, /* person swimming: medium-dark skin tone */
	{ 0x1F3CA, 0x1F3FF }, /* person swimming: dark skin tone */
	{ 0x1F3CB, 0x1F3FB }, /* person lifting weights: light skin tone */
	{ 0x1F3CB, 0x1F3FC }, /* person lifting weights: medium-light skin tone */
	{ 0x1F3CB, 0x1F3FD }, /* person lifting weights: medium skin tone */
	{ 0x1F3CB, 0x1F3FE }, /* person lifting weights: medium-dark skin tone */
	{ 0x1F3CB, 0x1F3FF }, /* person lifting weights: dark skin tone */
	{ 0x1F3CC, 0x1F3FB }, /* person golfing: light skin tone */
	{ 0x1F3CC, 0x1F3FC }, /* person golfing: medium-light skin tone */
	{ 0x1F3CC, 0x1F3FD }, /* person golfing: medium skin tone */
	{ 0x1F3CC, 0x1F3FE }, /* person golfing: medium-dark skin tone */
	{ 0x1F3CC, 0x1F3FF }, /* person golfing: dark skin tone */
	{ 0x1F442, 0x1F3FB }, /* ear: light skin tone */
	{ 0x1F442, 0x1F3FC }, /* ear: medium-light skin tone */
	{ 0x1F442, 0x1F3FD }, /* ear: medium skin tone */
	{ 0x1F442, 0x1F3FE }, /* ear: medium-dark skin tone */
	{ 0x1F442, 0x1F3FF }, /* ear: dark skin tone */
	{ 0x1F443, 0x1F3FB }, /* nose: light skin tone */
	{ 0x1F443, 0x1F3FC }, /* nose: medium-light skin tone */
	{ 0x1F443, 0x1F3FD }, /* nose: medium skin tone */
	{ 0x1F443, 0x1F3FE }, /* nose: medium-dark skin tone */
	{ 0x1F443, 0x1F3FF }, /* nose: dark skin tone */
	{ 0x1F446, 0x1F3FB }, /* backhand index pointing up: light skin tone */
	{ 0x1F446, 0x1F3FC }, /* backhand index pointing up: medium-light skin tone */
	{ 0x1F446, 0x1F3FD }, /* backhand index pointing up: medium skin tone */
	{ 0x1F446, 0x1F3FE }, /* backhand index pointing up: medium-dark skin tone */
	{ 0x1F446, 0x1F3FF }, /* backhand index pointing up: dark skin tone */
	{ 0x1F447, 0x1F3FB }, /* backhand index pointing down: light skin tone */
	{ 0x1F447, 0x1F3FC }, /* backhand index pointing down: medium-light skin tone */
	{ 0x1F447, 0x1F3FD }, /* backhand index pointing down: medium skin tone */
	{ 0x1F447, 0x1F3FE }, /* backhand index pointing down: medium-dark skin tone */
	{ 0x1F447, 0x1F3FF }, /* backhand index pointing down: dark skin tone */
	{ 0x1F448, 0x1F3FB }, /* backhand index pointing left: light skin tone */
	{ 0x1F448, 0x1F3FC }, /* backhand index pointing left: medium-light skin tone */
	{ 0x1F448, 0x1F3FD }, /* backhand index pointing left: medium skin tone */
	{ 0x1F448, 0x1F3FE }, /* backhand index pointing left: medium-dark skin tone */
	{ 0x1F448, 0x1F3FF }, /* backhand index pointing left: dark skin tone */
	{ 0x1F449, 0x1F3FB }, /* backhand index pointing right: light skin tone */
	{ 0x1F449, 0x1F3FC }, /* backhand index pointing right: medium-light skin tone */
	{ 0x1F449, 0x1F3FD }, /* backhand index pointing right: medium skin tone */
	{ 0x1F449, 0x1F3FE }, /* backhand index pointing right: medium-dark skin tone */
	{ 0x1F449, 0x1F3FF }, /* backhand index pointing right: dark skin tone */
	{ 0x1F44A, 0x1F3FB }, /* oncoming fist: light skin tone */
	{ 0x1F44A, 0x1F3FC }, /* oncoming fist: medium-light skin tone */
	{ 0x1F44A, 0x1F3FD }, /* oncoming fist: medium skin tone */
	{ 0x1F44A, 0x1F3FE }, /* oncoming fist: medium-dark skin tone */
	{ 0x1F44A, 0x1F3FF }, /* oncoming fist: dark skin tone */
	{ 0x1F44B, 0x1F3FB }, /* waving hand: light skin tone */
	{ 0x1F44B, 0x1F3FC }, /* waving hand: medium-light skin tone */
	{ 0x1F44B, 0x1F3FD }, /* waving hand: medium skin tone */
	{ 0x1F44B, 0x1F3FE }, /* waving hand: medium-dark skin tone */
	{ 0x1F44B, 0x1F3FF }, /* waving hand: dark skin tone */
	{ 0x1F44C, 0x1F3FB }, /* OK hand: light skin tone */
	{ 0x1F44C, 0x1F3FC }, /* OK hand: medium-light skin tone */
	{ 0x1F44C, 0x1F3FD }, /* OK hand: medium skin tone */
	{ 0x1F44C, 0x1F3FE }, /* OK hand: medium-dark skin tone */
	{ 0x1F44C, 0x1F3FF }, /* OK hand: dark skin tone */
	{ 0x1F44D, 0x1F3FB }, /* thumbs up: light skin tone */
	{ 0x1F44D, 0x1F3FC }, /* thumbs up: medium-light skin tone */
	{ 0x1F44D, 0x1F3FD }, /* thumbs up: medium skin tone */
	{ 0x1F44D, 0x1F3FE }, /* thumbs up: medium-dark skin tone */
	{ 0x1F44D, 0x1F3FF }, /* thumbs up: dark skin tone */
	{ 0x1F44E, 0x1F3FB }, /* thumbs down: light skin tone */
	{ 0x1F44E, 0x1F3FC }, /* thumbs down: medium-light skin tone */
	{ 0x1F44E, 0x1F3FD }, /* thumbs down: medium skin tone */
	{ 0x1F44E, 0x1F3FE }, /* thumbs down: medium-dark skin tone */
	{ 0x1F44E, 0x1F3FF }, /* thumbs down: dark skin tone */
	{ 0x1F44F, 0x1F3FB }, /* clapping hands: light skin tone */
	{ 0x1F44F, 0x1F3FC }, /* clapping hands: medium-light skin tone */
	{ 0x1F44F, 0x1F3FD }, /* clapping hands: medium skin tone */
	{ 0x1F44F, 0x1F3FE }, /* clapping hands: medium-dark skin tone */
	{ 0x1F44F, 0x1F3FF }, /* clapping hands: dark skin tone */
	{ 0x1F450, 0x1F3FB }, /* open hands: light skin tone */
	{ 0x1F450, 0x1F3FC }, /* open hands: medium-light skin tone */
	{ 0x1F450, 0x1F3FD }, /* open hands: medium skin tone */
	{ 0x1F450, 0x1F3FE }, /* open hands: medium-dark skin tone */
	{ 0x1F450, 0x1F3FF }, /* open hands: dark skin tone */
	{ 0x1F466, 0x1F3FB }, /* boy: light skin tone */
	{ 0x1F466, 0x1F3FC }, /* boy: medium-light skin tone */
	{ 0x1F466, 0x1F3FD }, /* boy: medium skin tone */
	{ 0x1F466, 0x1F3FE }, /* boy: medium-dark skin tone */
	{ 0x1F466, 0x1F3FF }, /* boy: dark skin tone */
	{ 0x1F467, 0x1F3FB }, /* girl: light skin tone */
	{ 0x1F467, 0x1F3FC }, /* girl: medium-light skin tone */
	{ 0x1F467, 0x1F3FD }, /* girl: medium skin tone */
	{ 0x1F467, 0x1F3FE }, /* girl: medium-dark skin tone */
	{ 0x1F467, 0x1F3FF }, /* girl: dark skin tone */
	{ 0x1F468, 0x1F3FB }, /* man: light skin tone */
	{ 0x1F468, 0x1F3FC }, /* man: medium-light skin tone */
	{ 0x1F468, 0x1F3FD }, /* man: medium skin tone */
	{ 0x1F468, 0x1F3FE }, /* man: medium-dark skin tone */
	{ 0x1F468, 0x1F3FF }, /* man: dark skin tone */
	{ 0x1F469, 0x1F3FB }, /* woman: light skin tone */
	{ 0x1F469, 0x1F3FC }, /* woman: medium-light skin tone */
	{ 0x1F469, 0x1F3FD }, /* woman: medium skin tone */
	{ 0x1F469, 0x1F3FE }, /* woman: medium-dark skin tone */
	{ 0x1F469, 0x1F3FF }, /* woman: dark skin tone */
	{ 0x1F46B, 0x1F3FB }, /* woman and man holding hands: light skin tone */
	{ 0x1F46B, 0x1F3FC }, /* woman and man holding hands: medium-light skin tone */
	{ 0x1F46B, 0x1F3FD }, /* woman and man holding hands: medium skin tone */
	{ 0x1F46B, 0x1F3FE }, /* woman and man holding hands: medium-dark skin tone */
	{ 0x1F46B, 0x1F3FF }, /* woman and man holding hands: dark skin tone */
	{ 0x1F46C, 0x1F3FB }, /* men holding hands: light skin tone */
	{ 0x1F46C, 0x1F3FC }, /* men holding hands: medium-light skin tone */
	{ 0x1F46C, 0x1F3FD }, /* men holding hands: medium skin tone */
	{ 0x1F46C, 0x1F3FE }, /* men holding hands: medium-dark skin tone */
	{ 0x1F46C, 0x1F3FF }, /* men holding hands: dark skin tone */
	{ 0x1F46D, 0x1F3FB }, /* women holding hands: light skin tone */
	{ 0x1F46D, 0x1F3FC }, /* women holding hands: medium-light skin tone */
	{ 0x1F46D, 0x1F3FD }, /* women holding hands: medium skin tone */
	{ 0x1F46D, 0x1F3FE }, /* women holding hands: medium-dark skin tone */
	{ 0x1F46D, 0x1F3FF }, /* women holding hands: dark skin tone */
	{ 0x1F46E, 0x1F3FB }, /* police officer: light skin tone */
	{ 0x1F46E, 0x1F3FC }, /* police officer: medium-light skin tone */
	{ 0x1F46E, 0x1F3FD }, /* police officer: medium skin tone */
	{ 0x1F46E, 0x1F3FE }, /* police officer: medium-dark skin tone */
	{ 0x1F46E, 0x1F3FF }, /* police officer: dark skin tone */
	{ 0x1F470, 0x1F3FB }, /* person with veil: light skin tone */
	{ 0x1F470, 0x1F3FC }, /* person with veil: medium-light skin tone */
	{ 0x1F470, 0x1F3FD }, /* person with veil: medium skin tone */
	{ 0x1F470, 0x1F3FE }, /* person with veil: medium-dark skin tone */
	{ 0x1F470, 0x1F3FF }, /* person with veil: dark skin tone */
	{ 0x1F471, 0x1F3FB }, /* person: light skin tone, blond hair */
	{ 0x1F471, 0x1F3FC }, /* person: medium-light skin tone, blond hair */
	{ 0x1F471, 0x1F3FD }, /* person: medium skin tone, blond hair */
	{ 0x1F471, 0x1F3FE }, /* person: medium-dark skin tone, blond hair */
	{ 0x1F471, 0x1F3FF }, /* person: dark skin tone, blond hair */
	{ 0x1F472, 0x1F3FB }, /* person with skullcap: light skin tone */
	{ 0x1F472, 0x1F3FC }, /* person with skullcap: medium-light skin tone */
	{ 0x1F472, 0x1F3FD }, /* person with skullcap: medium skin tone */
	{ 0x1F472, 0x1F3FE }, /* person with skullcap: medium-dark skin tone */
	{ 0x1F472, 0x1F3FF }, /* person with skullcap: dark skin tone */
	{ 0x1F473, 0x1F3FB }, /* person wearing turban: light skin tone */
	{ 0x1F473, 0x1F3FC }, /* person wearing turban: medium-light skin tone */
	{ 0x1F473, 0x1F3FD }, /* person wearing turban: medium skin tone */
	{ 0x1F473, 0x1F3FE }, /* person wearing turban: medium-dark skin tone */
	{ 0x1F473, 0x1F3FF }, /* person wearing turban: dark skin tone */
	{ 0x1F474, 0x1F3FB }, /* old man: light skin tone */
	{ 0x1F474, 0x1F3FC }, /* old man: medium-light skin tone */
	{ 0x1F474, 0x1F3FD }, /* old man: medium skin tone */
	{ 0x1F474, 0x1F3FE }, /* old man: medium-dark skin tone */
	{ 0x1F474, 0x1F3FF }, /* old man: dark skin tone */
	{ 0x1F475, 0x1F3FB }, /* old woman: light skin tone */
	{ 0x1F475, 0x1F3FC }, /* old woman: medium-light skin tone */
	{ 0x1F475, 0x1F3FD }, /* old woman: medium skin tone */
	{ 0x1F475, 0x1F3FE }, /* old woman: medium-dark skin tone */
	{ 0x1F475, 0x1F3FF }, /* old woman: dark skin tone */
	{ 0x1F476, 0x1F3FB }, /* baby: light skin tone */
	{ 0x1F476, 0x1F3FC }, /* baby: medium-light skin tone */
	{ 0x1F476, 0x1F3FD }, /* baby: medium skin tone */
	{ 0x1F476, 0x1F3FE }, /* baby: medium-dark skin tone */
	{ 0x1F476, 0x1F3FF }, /* baby: dark skin tone */
	{ 0x1F477, 0x1F3FB }, /* construction worker: light skin tone */
	{ 0x1F477, 0x1F3FC }, /* construction worker: medium-light skin tone */
	{ 0x1F477, 0x1F3FD }, /* construction worker: medium skin tone */
	{ 0x1F477, 0x1F3FE }, /* construction worker: medium-dark skin tone */
	{ 0x1F477, 0x1F3FF }, /* construction worker: dark skin tone */
	{ 0x1F478, 0x1F3FB }, /* princess: light skin tone */
	{ 0x1F478, 0x1F3FC }, /* princess: medium-light skin tone */
	{ 0x1F478, 0x1F3FD }, /* princess: medium skin tone */
	{ 0x1F478, 0x1F3FE }, /* princess: medium-dark skin tone */
	{ 0x1F478, 0x1F3FF }, /* princess: dark skin tone */
	{ 0x1F47C, 0x1F3FB }, /* baby angel: light skin tone */
	{ 0x1F47C, 0x1F3FC }, /* baby angel: medium-light skin tone */
	{ 0x1F47C, 0x1F3FD }, /* baby angel: medium skin tone */
	{ 0x1F47C, 0x1F3FE }, /* baby angel: medium-dark skin tone */
	{ 0x1F47C, 0x1F3FF }, /* baby angel: dark skin tone */
	{ 0x1F481, 0x1F3FB }, /* person tipping hand: light skin tone */
	{ 0x1F481, 0x1F3FC }, /* person tipping hand: medium-light skin tone */
	{ 0x1F481, 0x1F3FD }, /* person tipping hand: medium skin tone */
	{ 0x1F481, 0x1F3FE }, /* person tipping hand: medium-dark skin tone */
	{ 0x1F481, 0x1F3FF }, /* person tipping hand: dark skin tone */
	{ 0x1F482, 0x1F3FB }, /* guard: light skin tone */
	{ 0x1F482, 0x1F3FC }, /* guard: medium-light skin tone */
	{ 0x1F482, 0x1F3FD }, /* guard: medium skin tone */
	{ 0x1F482, 0x1F3FE }, /* guard: medium-dark skin tone */
	{ 0x1F482, 0x1F3FF }, /* guard: dark skin tone */
	{ 0x1F483, 0x1F3FB }, /* woman dancing: light skin tone */
	{ 0x1F483, 0x1F3FC }, /* woman dancing: medium-light skin tone */
	{ 0x1F483, 0x1F3FD }, /* woman dancing: medium skin tone */
	{ 0x1F483, 0x1F3FE }, /* woman dancing: medium-dark skin tone */
	{ 0x1F483, 0x1F3FF }, /* woman dancing: dark skin tone */
	{ 0x1F485, 0x1F3FB }, /* nail polish: light skin tone */
	{ 0x1F485, 0x1F3FC }, /* nail polish: medium-light skin tone */
	{ 0x1F485, 0x1F3FD }, /* nail polish: medium skin tone */
	{ 0x1F485, 0x1F3FE }, /* nail polish: medium-dark skin tone */
	{ 0x1F485, 0x1F3FF }, /* nail polish: dark skin tone */
	{ 0x1F486, 0x1F3FB }, /* person getting massage: light skin tone */
	{ 0x1F486, 0x1F3FC }, /* person getting massage: medium-light skin tone */
	{ 0x1F486, 0x1F3FD }, /* person getting massage: medium skin tone */
	{ 0x1F486, 0x1F3FE }, /* person getting massage: medium-dark skin tone */
	{ 0x1F486, 0x1F3FF }, /* person getting massage: dark skin tone */
	{ 0x1F487, 0x1F3FB }, /* person getting haircut: light skin tone */
	{ 0x1F487, 0x1F3FC }, /* person getting haircut: medium-light skin tone */
	{ 0x1F487, 0x1F3FD }, /* person getting haircut: medium skin tone */
	{ 0x1F487, 0x1F3FE }, /* person getting haircut: medium-dark skin tone */
	{ 0x1F487, 0x1F3FF }, /* person getting haircut: dark skin tone */
	{ 0x1F48F, 0x1F3FB }, /* kiss: light skin tone */
	{ 0x1F48F, 0x1F3FC }, /* kiss: medium-light skin tone */
	{ 0x1F48F, 0x1F3FD }, /* kiss: medium skin tone */
	{ 0x1F48F, 0x1F3FE }, /* kiss: medium-dark skin tone */
	{ 0x1F48F, 0x1F3FF }, /* kiss: dark skin tone */
	{ 0x1F491, 0x1F3FB }, /* couple with heart: light skin tone */
	{ 0x1F491, 0x1F3FC }, /* couple with heart: medium-light skin tone */
	{ 0x1F491, 0x1F3FD }, /* couple with heart: medium skin tone */
	{ 0x1F491, 0x1F3FE }, /* couple with heart: medium-dark skin tone */
	{ 0x1F491, 0x1F3FF }, /* couple with heart: dark skin tone */
	{ 0x1F4AA, 0x1F3FB }, /* flexed biceps: light skin tone */
	{ 0x1F4AA, 0x1F3FC }, /* flexed biceps: medium-light skin tone */
	{ 0x1F4AA, 0x1F3FD }, /* flexed biceps: medium skin tone */
	{ 0x1F4AA, 0x1F3FE }, /* flexed biceps: medium-dark skin tone */
	{ 0x1F4AA, 0x1F3FF }, /* flexed biceps: dark skin tone */
	{ 0x1F574, 0x1F3FB }, /* person in suit levitating: light skin tone */
	{ 0x1F574, 0x1F3FC }, /* person in suit levitating: medium-light skin tone */
	{ 0x1F574, 0x1F3FD }, /* person in suit levitating: medium skin tone */
	{ 0x1F574, 0x1F3FE }, /* person in suit levitating: medium-dark skin tone */
	{ 0x1F574, 0x1F3FF }, /* person in suit levitating: dark skin tone */
	{ 0x1F575, 0x1F3FB }, /* detective: light skin tone */
	{ 0x1F575, 0x1F3FC }, /* detective: medium-light skin tone */
	{ 0x1F575, 0x1F3FD }, /* detective: medium skin tone */
	{ 0x1F575, 0x1F3FE }, /* detective: medium-dark skin tone */
	{ 0x1F575, 0x1F3FF }, /* detective: dark skin tone */
	{ 0x1F57A, 0x1F3FB }, /* man dancing: light skin tone */
	{ 0x1F57A, 0x1F3FC }, /* man dancing: medium-light skin tone */
	{ 0x1F57A, 0x1F3FD }, /* man dancing: medium skin tone */
	{ 0x1F57A, 0x1F3FE }, /* man dancing: medium-dark skin tone */
	{ 0x1F57A, 0x1F3FF }, /* man dancing: dark skin tone */
	{ 0x1F590, 0x1F3FB }, /* hand with fingers splayed: light skin tone */
	{ 0x1F590, 0x1F3FC }, /* hand with fingers splayed: medium-light skin tone */
	{ 0x1F590, 0x1F3FD }, /* hand with fingers splayed: medium skin tone */
	{ 0x1F590, 0x1F3FE }, /* hand with fingers splayed: medium-dark skin tone */
	{ 0x1F590, 0x1F3FF }, /* hand with fingers splayed: dark skin tone */
	{ 0x1F595, 0x1F3FB }, /* middle finger: light skin tone */
	{ 0x1F595, 0x1F3FC }, /* middle finger: medium-light skin tone */
	{ 0x1F595, 0x1F3FD }, /* middle finger: medium skin tone */
	{ 0x1F595, 0x1F3FE }, /* middle finger: medium-dark skin tone */
	{ 0x1F595, 0x1F3FF }, /* middle finger: dark skin tone */
	{ 0x1F596, 0x1F3FB }, /* vulcan salute: light skin tone */
	{ 0x1F596, 0x1F3FC }, /* vulcan salute: medium-light skin tone */
	{ 0x1F596, 0x1F3FD }, /* vulcan salute: medium skin tone */
	{ 0x1F596, 0x1F3FE }, /* vulcan salute: medium-dark skin tone */
	{ 0x1F596, 0x1F3FF }, /* vulcan salute: dark skin tone */
	{ 0x1F645, 0x1F3FB }, /* person gesturing NO: light skin tone */
	{ 0x1F645, 0x1F3FC }, /* person gesturing NO: medium-light skin tone */
	{ 0x1F645, 0x1F3FD }, /* person gesturing NO: medium skin tone */
	{ 0x1F645, 0x1F3FE }, /* person gesturing NO: medium-dark skin tone */
	{ 0x1F645, 0x1F3FF }, /* person gesturing NO: dark skin tone */
	{ 0x1F646, 0x1F3FB }, /* person gesturing OK: light skin tone */
	{ 0x1F646, 0x1F3FC }, /* person gesturing OK: medium-light skin tone */
	{ 0x1F646, 0x1F3FD }, /* person gesturing OK: medium skin tone */
	{ 0x1F646, 0x1F3FE }, /* person gesturing OK: medium-dark skin tone */
	{ 0x1F646, 0x1F3FF }, /* person gesturing OK: dark skin tone */
	{ 0x1F647, 0x1F3FB }, /* person bowing: light skin tone */
	{ 0x1F647, 0x1F3FC }, /* person bowing: medium-light skin tone */
	{ 0x1F647, 0x1F3FD }, /* person bowing: medium skin tone */
	{ 0x1F647, 0x1F3FE }, /* person bowing: medium-dark skin tone */
	{ 0x1F647, 0x1F3FF }, /* person bowing: dark skin tone */
	{ 0x1F64B, 0x1F3FB }, /* person raising hand: light skin tone */
	{ 0x1F64B, 0x1F3FC }, /* person raising hand: medium-light skin tone */
	{ 0x1F64B, 0x1F3FD }, /* person raising hand: medium skin tone */
	{ 0x1F64B, 0x1F3FE }, /* person raising hand: medium-dark skin tone */
	{ 0x1F64B, 0x1F3FF }, /* person raising hand: dark skin tone */
	{ 0x1F64C, 0x1F3FB }, /* raising hands: light skin tone */
	{ 0x1F64C, 0x1F3FC }, /* raising hands: medium-light skin tone */
	{ 0x1F64C, 0x1F3FD }, /* raising hands: medium skin tone */
	{ 0x1F64C, 0x1F3FE }, /* raising hands: medium-dark skin tone */
	{ 0x1F64C, 0x1F3FF }, /* raising hands: dark skin tone */
	{ 0x1F64D, 0x1F3FB }, /* person frowning: light skin tone */
	{ 0x1F64D, 0x1F3FC }, /* person frowning: medium-light skin tone */
	{ 0x1F64D, 0x1F3FD }, /* person frowning: medium skin tone */
	{ 0x1F64D, 0x1F3FE }, /* person frowning: medium-dark skin tone */
	{ 0x1F64D, 0x1F3FF }, /* person frowning: dark skin tone */
	{ 0x1F64E, 0x1F3FB }, /* person pouting: light skin tone */
	{ 0x1F64E, 0x1F3FC }, /* person pouting: medium-light skin tone */
	{ 0x1F64E, 0x1F3FD }, /* person pouting: medium skin tone */
	{ 0x1F64E, 0x1F3FE }, /* person pouting: medium-dark skin tone */
	{ 0x1F64E, 0x1F3FF }, /* person pouting: dark skin tone */
	{ 0x1F64F, 0x1F3FB }, /* folded hands: light skin tone */
	{ 0x1F64F, 0x1F3FC }, /* folded hands: medium-light skin tone */
	{ 0x1F64F, 0x1F3FD }, /* folded hands: medium skin tone */
	{ 0x1F64F, 0x1F3FE }, /* folded hands: medium-dark skin tone */
	{ 0x1F64F, 0x1F3FF }, /* folded hands: dark skin tone */
	{ 0x1F6A3, 0x1F3FB }, /* person rowing boat: light skin tone */
	{ 0x1F6A3, 0x1F3FC }, /* person rowing boat: medium-light skin tone */
	{ 0x1F6A3, 0x1F3FD }, /* person rowing boat: medium skin tone */
	{ 0x1F6A3, 0x1F3FE }, /* person rowing boat: medium-dark skin tone */
	{ 0x1F6A3, 0x1F3FF }, /* person rowing boat: dark skin tone */
	{ 0x1F6B4, 0x1F3FB }, /* person biking: light skin tone */
	{ 0x1F6B4, 0x1F3FC }, /* person biking: medium-light skin tone */
	{ 0x1F6B4, 0x1F3FD }, /* person biking: medium skin tone */
	{ 0x1F6B4, 0x1F3FE }, /* person biking: medium-dark skin tone */
	{ 0x1F6B4, 0x1F3FF }, /* person biking: dark skin tone */
	{ 0x1F6B5, 0x1F3FB }, /* person mountain biking: light skin tone */
	{ 0x1F6B5, 0x1F3FC }, /* person mountain biking: medium-light skin tone */
	{ 0x1F6B5, 0x1F3FD }, /* person mountain biking: medium skin tone */
	{ 0x1F6B5, 0x1F3FE }, /* person mountain biking: medium-dark skin tone */
	{ 0x1F6B5, 0x1F3FF }, /* person mountain biking: dark skin tone */
	{ 0x1F6B6, 0x1F3FB }, /* person walking: light skin tone */
	{ 0x1F6B6, 0x1F3FC }, /* person walking: medium-light skin tone */
	{ 0x1F6B6, 0x1F3FD }, /* person walking: medium skin tone */
	{ 0x1F6B6, 0x1F3FE }, /* person walking: medium-dark skin tone */
	{ 0x1F6B6, 0x1F3FF }, /* person walking: dark skin tone */
	{ 0x1F6C0, 0x1F3FB }, /* person taking bath: light skin tone */
	{ 0x1F6C0, 0x1F3FC }, /* person taking bath: medium-light skin tone */
	{ 0x1F6C0, 0x1F3FD }, /* person taking bath: medium skin tone */
	{ 0x1F6C0, 0x1F3FE }, /* person taking bath: medium-dark skin tone */
	{ 0x1F6C0, 0x1F3FF }, /* person taking bath: dark skin tone */
	{ 0x1F6CC, 0x1F3FB }, /* person in bed: light skin tone */
	{ 0x1F6CC, 0x1F3FC }, /* person in bed: medium-light skin tone */
	{ 0x1F6CC, 0x1F3FD }, /* person in bed: medium skin tone */
	{ 0x1F6CC, 0x1F3FE }, /* person in bed: medium-dark skin tone */
	{ 0x1F6CC, 0x1F3FF }, /* person in bed: dark skin tone */
	{ 0x1F90C, 0x1F3FB }, /* pinched fingers: light skin tone */
	{ 0x1F90C, 0x1F3FC }, /* pinched fingers: medium-light skin tone */
	{ 0x1F90C, 0x1F3FD }, /* pinched fingers: medium skin tone */
	{ 0x1F90C, 0x1F3FE }, /* pinched fingers: medium-dark skin tone */
	{ 0x1F90C, 0x1F3FF }, /* pinched fingers: dark skin tone */
	{ 0x1F90F, 0x1F3FB }, /* pinching hand: light skin tone */
	{ 0x1F90F, 0x1F3FC }, /* pinching hand: medium-light skin tone */
	{ 0x1F90F, 0x1F3FD }, /* pinching hand: medium skin tone */
	{ 0x1F90F, 0x1F3FE }, /* pinching hand: medium-dark skin tone */
	{ 0x1F90F, 0x1F3FF }, /* pinching hand: dark skin tone */
	{ 0x1F918, 0x1F3FB }, /* sign of the horns: light skin tone */
	{ 0x1F918, 0x1F3FC }, /* sign of the horns: medium-light skin tone */
	{ 0x1F918, 0x1F3FD }, /* sign of the horns: medium skin tone */
	{ 0x1F918, 0x1F3FE }, /* sign of the horns: medium-dark skin tone */
	{ 0x1F918, 0x1F3FF }, /* sign of the horns: dark skin tone */
	{ 0x1F919, 0x1F3FB }, /* call me hand: light skin tone */
	{ 0x1F919, 0x1F3FC }, /* call me hand: medium-light skin tone */
	{ 0x1F919, 0x1F3FD }, /* call me hand: medium skin tone */
	{ 0x1F919, 0x1F3FE }, /* call me hand: medium-dark skin tone */
	{ 0x1F919, 0x1F3FF }, /* call me hand: dark skin tone */
	{ 0x1F91A, 0x1F3FB }, /* raised back of hand: light skin tone */
	{ 0x1F91A, 0x1F3FC }, /* raised back of hand: medium-light skin tone */
	{ 0x1F91A, 0x1F3FD }, /* raised back of hand: medium skin tone */
	{ 0x1F91A, 0x1F3FE }, /* raised back of hand: medium-dark skin tone */
	{ 0x1F91A, 0x1F3FF }, /* raised back of hand: dark skin tone */
	{ 0x1F91B, 0x1F3FB }, /* left-facing fist: light skin tone */
	{ 0x1F91B, 0x1F3FC }, /* left-facing fist: medium-light skin tone */
	{ 0x1F91B, 0x1F3FD }, /* left-facing fist: medium skin tone */
	{ 0x1F91B, 0x1F3FE }, /* left-facing fist: medium-dark skin tone */
	{ 0x1F91B, 0x1F3FF }, /* left-facing fist: dark skin tone */
	{ 0x1F91C, 0x1F3FB }, /* right-facing fist: light skin tone */
	{ 0x1F91C, 0x1F3FC }, /* right-facing fist: medium-light skin tone */
	{ 0x1F91C, 0x1F3FD }, /* right-facing fist: medium skin tone */
	{ 0x1F91C, 0x1F3FE }, /* right-facing fist: medium-dark skin tone */
	{ 0x1F91C, 0x1F3FF }, /* right-facing fist: dark skin tone */
	{ 0x1F91D, 0x1F3FB }, /* handshake: light skin tone */
	{ 0x1F91D, 0x1F3FC }, /* handshake: medium-light skin tone */
	{ 0x1F91D, 0x1F3FD }, /* handshake: medium skin tone */
	{ 0x1F91D, 0x1F3FE }, /* handshake: medium-dark skin tone */
	{ 0x1F91D, 0x1F3FF }, /* handshake: dark skin tone */
	{ 0x1F91E, 0x1F3FB }, /* crossed fingers: light skin tone */
	{ 0x1F91E, 0x1F3FC }, /* crossed fingers: medium-light skin tone */
	{ 0x1F91E, 0x1F3FD }, /* crossed fingers: medium skin tone */
	{ 0x1F91E, 0x1F3FE }, /* crossed fingers: medium-dark skin tone */
	{ 0x1F91E, 0x1F3FF }, /* crossed fingers: dark skin tone */
	{ 0x1F91F, 0x1F3FB }, /* love-you gesture: light skin tone */
	{ 0x1F91F, 0x1F3FC }, /* love-you gesture: medium-light skin tone */
	{ 0x1F91F, 0x1F3FD }, /* love-you gesture: medium skin tone */
	{ 0x1F91F, 0x1F3FE }, /* love-you gesture: medium-dark skin tone */
	{ 0x1F91F, 0x1F3FF }, /* love-you gesture: dark skin tone */
	{ 0x1F926, 0x1F3FB }, /* person facepalming: light skin tone */
	{ 0x1F926, 0x1F3FC }, /* person facepalming: medium-light skin tone */
	{ 0x1F926, 0x1F3FD }, /* person facepalming: medium skin tone */
	{ 0x1F926, 0x1F3FE }, /* person facepalming: medium-dark skin tone */
	{ 0x1F926, 0x1F3FF }, /* person facepalming: dark skin tone */
	{ 0x1F930, 0x1F3FB }, /* pregnant woman: light skin tone */
	{ 0x1F930, 0x1F3FC }, /* pregnant woman: medium-light skin tone */
	{ 0x1F930, 0x1F3FD }, /* pregnant woman: medium skin tone */
	{ 0x1F930, 0x1F3FE }, /* pregnant woman: medium-dark skin tone */
	{ 0x1F930, 0x1F3FF }, /* pregnant woman: dark skin tone */
	{ 0x1F931, 0x1F3FB }, /* breast-feeding: light skin tone */
	{ 0x1F931, 0x1F3FC }, /* breast-feeding: medium-light skin tone */
	{ 0x1F931, 0x1F3FD }, /* breast-feeding: medium skin tone */
	{ 0x1F931, 0x1F3FE }, /* breast-feeding: medium-dark skin tone */
	{ 0x1F931, 0x1F3FF }, /* breast-feeding: dark skin tone */
	{ 0x1F932, 0x1F3FB }, /* palms up together: light skin tone */
	{ 0x1F932, 0x1F3FC }, /* palms up together: medium-light skin tone */
	{ 0x1F932, 0x1F3FD }, /* palms up together: medium skin tone */
	{ 0x1F932, 0x1F3FE }, /* palms up together: medium-dark skin tone */
	{ 0x1F932, 0x1F3FF }, /* palms up together: dark skin tone */
	{ 0x1F933, 0x1F3FB }, /* selfie: light skin tone */
	{ 0x1F933, 0x1F3FC }, /* selfie: medium-light skin tone */
	{ 0x1F933, 0x1F3FD }, /* selfie: medium skin tone */
	{ 0x1F933, 0x1F3FE }, /* selfie: medium-dark skin tone */
	{ 0x1F933, 0x1F3FF }, /* selfie: dark skin tone */
	{ 0x1F934, 0x1F3FB }, /* prince: light skin tone */
	{ 0x1F934, 0x1F3FC }, /* prince: medium-light skin tone */
	{ 0x1F934, 0x1F3FD }, /* prince: medium skin tone */
	{ 0x1F934, 0x1F3FE }, /* prince: medium-dark skin tone */
	{ 0x1F934, 0x1F3FF }, /* prince: dark skin tone */
	{ 0x1F935, 0x1F3FB }, /* person in tuxedo: light skin tone */
	{ 0x1F935, 0x1F3FC }, /* person in tuxedo: medium-light skin tone */
	{ 0x1F935, 0x1F3FD }, /* person in tuxedo: medium skin tone */
	{ 0x1F935, 0x1F3FE }, /* person in tuxedo: medium-dark skin tone */
	{ 0x1F935, 0x1F3FF }, /* person in tuxedo: dark skin tone */
	{ 0x1F936, 0x1F3FB }, /* Mrs. Claus: light skin tone */
	{ 0x1F936, 0x1F3FC }, /* Mrs. Claus: medium-light skin tone */
	{ 0x1F936, 0x1F3FD }, /* Mrs. Claus: medium skin tone */
	{ 0x1F936, 0x1F3FE }, /* Mrs. Claus: medium-dark skin tone */
	{ 0x1F936, 0x1F3FF }, /* Mrs. Claus: dark skin tone */
	{ 0x1F937, 0x1F3FB }, /* person shrugging: light skin tone */
	{ 0x1F937, 0x1F3FC }, /* person shrugging: medium-light skin tone */
	{ 0x1F937, 0x1F3FD }, /* person shrugging: medium skin tone */
	{ 0x1F937, 0x1F3FE }, /* person shrugging: medium-dark skin tone */
	{ 0x1F937, 0x1F3FF }, /* person shrugging: dark skin tone */
	{ 0x1F938, 0x1F3FB }, /* person cartwheeling: light skin tone */
	{ 0x1F938, 0x1F3FC }, /* person cartwheeling: medium-light skin tone */
	{ 0x1F938, 0x1F3FD }, /* person cartwheeling: medium skin tone */
	{ 0x1F938, 0x1F3FE }, /* person cartwheeling: medium-dark skin tone */
	{ 0x1F938, 0x1F3FF }, /* person cartwheeling: dark skin tone */
	{ 0x1F939, 0x1F3FB }, /* person juggling: light skin tone */
	{ 0x1F939, 0x1F3FC }, /* person juggling: medium-light skin tone */
	{ 0x1F939, 0x1F3FD }, /* person juggling: medium skin tone */
	{ 0x1F939, 0x1F3FE }, /* person juggling: medium-dark skin tone */
	{ 0x1F939, 0x1F3FF }, /* person juggling: dark skin tone */
	{ 0x1F93D, 0x1F3FB }, /* person playing water polo: light skin tone */
	{ 0x1F93D, 0x1F3FC }, /* person playing water polo: medium-light skin tone */
	{ 0x1F93D, 0x1F3FD }, /* person playing water polo: medium skin tone */
	{ 0x1F93D, 0x1F3FE }, /* person playing water polo: medium-dark skin tone */
	{ 0x1F93D, 0x1F3FF }, /* person playing water polo: dark skin tone */
	{ 0x1F93E, 0x1F3FB }, /* person playing handball: light skin tone */
	{ 0x1F93E, 0x1F3FC }, /* person playing handball: medium-light skin tone */
	{ 0x1F93E, 0x1F3FD }, /* person playing handball: medium skin tone */
	{ 0x1F93E, 0x1F3FE }, /* person playing handball: medium-dark skin tone */
	{ 0x1F93E, 0x1F3FF }, /* person playing handball: dark skin tone */
	{ 0x1F977, 0x1F3FB }, /* ninja: light skin tone */
	{ 0x1F977, 0x1F3FC }, /* ninja: medium-light skin tone */
	{ 0x1F977, 0x1F3FD }, /* ninja: medium skin tone */
	{ 0x1F977, 0x1F3FE }, /* ninja: medium-dark skin tone */
	{ 0x1F977, 0x1F3FF }, /* ninja: dark skin tone */
	{ 0x1F9B5, 0x1F3FB }, /* leg: light skin tone */
	{ 0x1F9B5, 0x1F3FC }, /* leg: medium-light skin tone */
	{ 0x1F9B5, 0x1F3FD }, /* leg: medium skin tone */
	{ 0x1F9B5, 0x1F3FE }, /* leg: medium-dark skin tone */
	{ 0x1F9B5, 0x1F3FF }, /* leg: dark skin tone */
	{ 0x1F9B6, 0x1F3FB }, /* foot: light skin tone */
	{ 0x1F9B6, 0x1F3FC }, /* foot: medium-light skin tone */
	{ 0x1F9B6, 0x1F3FD }, /* foot: medium skin tone */
	{ 0x1F9B6, 0x1F3FE }, /* foot: medium-dark skin tone */
	{ 0x1F9B6, 0x1F3FF }, /* foot: dark skin tone */
	{ 0x1F9B8, 0x1F3FB }, /* superhero: light skin tone */
	{ 0x1F9B8, 0x1F3FC }, /* superhero: medium-light skin tone */
	{ 0x1F9B8, 0x1F3FD }, /* superhero: medium skin tone */
	{ 0x1F9B8, 0x1F3FE }, /* superhero: medium-dark skin tone */
	{ 0x1F9B8, 0x1F3FF }, /* superhero: dark skin tone */
	{ 0x1F9B9, 0x1F3FB }, /* supervillain: light skin tone */
	{ 0x1F9B9, 0x1F3FC }, /* supervillain: medium-light skin tone */
	{ 0x1F9B9, 0x1F3FD }, /* supervillain: medium skin tone */
	{ 0x1F9B9, 0x1F3FE }, /* supervillain: medium-dark skin tone */
	{ 0x1F9B9, 0x1F3FF }, /* supervillain: dark skin tone */
	{ 0x1F9BB, 0x1F3FB }, /* ear with hearing aid: light skin tone */
	{ 0x1F9BB, 0x1F3FC }, /* ear with hearing aid: medium-light skin tone */
	{ 0x1F9BB, 0x1F3FD }, /* ear with hearing aid: medium skin tone */
	{ 0x1F9BB, 0x1F3FE }, /* ear with hearing aid: medium-dark skin tone */
	{ 0x1F9BB, 0x1F3FF }, /* ear with hearing aid: dark skin tone */
	{ 0x1F9CD, 0x1F3FB }, /* person standing: light skin tone */
	{ 0x1F9CD, 0x1F3FC }, /* person standing: medium-light skin tone */
	{ 0x1F9CD, 0x1F3FD }, /* person standing: medium skin tone */
	{ 0x1F9CD, 0x1F3FE }, /* person standing: medium-dark skin tone */
	{ 0x1F9CD, 0x1F3FF }, /* person standing: dark skin tone */
	{ 0x1F9CE, 0x1F3FB }, /* person kneeling: light skin tone */
	{ 0x1F9CE, 0x1F3FC }, /* person kneeling: medium-light skin tone */
	{ 0x1F9CE, 0x1F3FD }, /* person kneeling: medium skin tone */
	{ 0x1F9CE, 0x1F3FE }, /* person kneeling: medium-dark skin tone */
	{ 0x1F9CE, 0x1F3FF }, /* person kneeling: dark skin tone */
	{ 0x1F9CF, 0x1F3FB }, /* deaf person: light skin tone */
	{ 0x1F9CF, 0x1F3FC }, /* deaf person: medium-light skin tone */
	{ 0x1F9CF, 0x1F3FD }, /* deaf person: medium skin tone */
	{ 0x1F9CF, 0x1F3FE }, /* deaf person: medium-dark skin tone */
	{ 0x1F9CF, 0x1F3FF }, /* deaf person: dark skin tone */
	{ 0x1F9D1, 0x1F3FB }, /* person: light skin tone */
	{ 0x1F9D1, 0x1F3FC }, /* person: medium-light skin tone */
	{ 0x1F9D1, 0x1F3FD }, /* person: medium skin tone */
	{ 0x1F9D1, 0x1F3FE }, /* person: medium-dark skin tone */
	{ 0x1F9D1, 0x1F3FF }, /* person: dark skin tone */
	{ 0x1F9D2, 0x1F3FB }, /* child: light skin tone */
	{ 0x1F9D2, 0x1F3FC }, /* child: medium-light skin tone */
	{ 0x1F9D2, 0x1F3FD }, /* child: medium skin tone */
	{ 0x1F9D2, 0x1F3FE }, /* child: medium-dark skin tone */
	{ 0x1F9D2, 0x1F3FF }, /* child: dark skin tone */
	{ 0x1F9D3, 0x1F3FB }, /* older person: light skin tone */
	{ 0x1F9D3, 0x1F3FC }, /* older person: medium-light skin tone */
	{ 0x1F9D3, 0x1F3FD }, /* older person: medium skin tone */
	{ 0x1F9D3, 0x1F3FE }, /* older person: medium-dark skin tone */
	{ 0x1F9D3, 0x1F3FF }, /* older person: dark skin tone */
	{ 0x1F9D4, 0x1F3FB }, /* person: light skin tone, beard */
	{ 0x1F9D4, 0x1F3FC }, /* person: medium-light skin tone, beard */
	{ 0x1F9D4, 0x1F3FD }, /* person: medium skin tone, beard */
	{ 0x1F9D4, 0x1F3FE }, /* person: medium-dark skin tone, beard */
	{ 0x1F9D4, 0x1F3FF }, /* person: dark skin tone, beard */
	{ 0x1F9D5, 0x1F3FB }, /* woman with headscarf: light skin tone */
	{ 0x1F9D5, 0x1F3FC }, /* woman with headscarf: medium-light skin tone */
	{ 0x1F9D5, 0x1F3FD }, /* woman with headscarf: medium skin tone */
	{ 0x1F9D5, 0x1F3FE }, /* woman with headscarf: medium-dark skin tone */
	{ 0x1F9D5, 0x1F3FF }, /* woman with headscarf: dark skin tone */
	{ 0x1F9D6, 0x1F3FB }, /* person in steamy room: light skin tone */
	{ 0x1F9D6, 0x1F3FC }, /* person in steamy room: medium-light skin tone */
	{ 0x1F9D6, 0x1F3FD }, /* person in steamy room: medium skin tone */
	{ 0x1F9D6, 0x1F3FE }, /* person in steamy room: medium-dark skin tone */
	{ 0x1F9D6, 0x1F3FF }, /* person in steamy room: dark skin tone */
	{ 0x1F9D7, 0x1F3FB }, /* person climbing: light skin tone */
	{ 0x1F9D7, 0x1F3FC }, /* person climbing: medium-light skin tone */
	{ 0x1F9D7, 0x1F3FD }, /* person climbing: medium skin tone */
	{ 0x1F9D7, 0x1F3FE }, /* person climbing: medium-dark skin tone */
	{ 0x1F9D7, 0x1F3FF }, /* person climbing: dark skin tone */
	{ 0x1F9D8, 0x1F3FB }, /* person in lotus position: light skin tone */
	{ 0x1F9D8, 0x1F3FC }, /* person in lotus position: medium-light skin tone */
	{ 0x1F9D8, 0x1F3FD }, /* person in lotus position: medium skin tone */
	{ 0x1F9D8, 0x1F3FE }, /* person in lotus position: medium-dark skin tone */
	{ 0x1F9D8, 0x1F3FF }, /* person in lotus position: dark skin tone */
	{ 0x1F9D9, 0x1F3FB }, /* mage: light skin tone */
	{ 0x1F9D9, 0x1F3FC }, /* mage: medium-light skin tone */
	{ 0x1F9D9, 0x1F3FD }, /* mage: medium skin tone */
	{ 0x1F9D9, 0x1F3FE }, /* mage: medium-dark skin tone */
	{ 0x1F9D9, 0x1F3FF }, /* mage: dark skin tone */
	{ 0x1F9DA, 0x1F3FB }, /* fairy: light skin tone */
	{ 0x1F9DA, 0x1F3FC }, /* fairy: medium-light skin tone */
	{ 0x1F9DA, 0x1F3FD }, /* fairy: medium skin tone */
	{ 0x1F9DA, 0x1F3FE }, /* fairy: medium-dark skin tone */
	{ 0x1F9DA, 0x1F3FF }, /* fairy: dark skin tone */
	{ 0x1F9DB, 0x1F3FB }, /* vampire: light skin tone */
	{ 0x1F9DB, 0x1F3FC }, /* vampire: medium-light skin tone */
	{ 0x1F9DB, 0x1F3FD }, /* vampire: medium skin tone */
	{ 0x1F9DB, 0x1F3FE }, /* vampire: medium-dark skin tone */
	{ 0x1F9DB, 0x1F3FF }, /* vampire: dark skin tone */
	{ 0x1F9DC, 0x1F3FB }, /* merperson: light skin tone */
	{ 0x1F9DC, 0x1F3FC }, /* merperson: medium-light skin tone */
	{ 0x1F9DC, 0x1F3FD }, /* merperson: medium skin tone */
	{ 0x1F9DC, 0x1F3FE }, /* merperson: medium-dark skin tone */
	{ 0x1F9DC, 0x1F3FF }, /* merperson: dark skin tone */
	{ 0x1F9DD, 0x1F3FB }, /* elf: light skin tone */
	{ 0x1F9DD, 0x1F3FC }, /* elf: medium-light skin tone */
	{ 0x1F9DD, 0x1F3FD }, /* elf: medium skin tone */
	{ 0x1F9DD, 0x1F3FE }, /* elf: medium-dark skin tone */
	{ 0x1F9DD, 0x1F3FF }, /* elf: dark skin tone */
	{ 0x1FAC3, 0x1F3FB }, /* pregnant man: light skin tone */
	{ 0x1FAC3, 0x1F3FC }, /* pregnant man: medium-light skin tone */
	{ 0x1FAC3, 0x1F3FD }, /* pregnant man: medium skin tone */
	{ 0x1FAC3, 0x1F3FE }, /* pregnant man: medium-dark skin tone */
	{ 0x1FAC3, 0x1F3FF }, /* pregnant man: dark skin tone */
	{ 0x1FAC4, 0x1F3FB }, /* pregnant person: light skin tone */
	{ 0x1FAC4, 0x1F3FC }, /* pregnant person: medium-light skin tone */
	{ 0x1FAC4, 0x1F3FD }, /* pregnant person: medium skin tone */
	{ 0x1FAC4, 0x1F3FE }, /* pregnant person: medium-dark skin tone */
	{ 0x1FAC4, 0x1F3FF }, /* pregnant person: dark skin tone */
	{ 0x1FAC5, 0x1F3FB }, /* person with crown: light skin tone */
	{ 0x1FAC5, 0x1F3FC }, /* person with crown: medium-light skin tone */
	{ 0x1FAC5, 0x1F3FD }, /* person with crown: medium skin tone */
	{ 0x1FAC5, 0x1F3FE }, /* person with crown: medium-dark skin tone */
	{ 0x1FAC5, 0x1F3FF }, /* person with crown: dark skin tone */
	{ 0x1FAF0, 0x1F3FB }, /* hand with index finger and thumb crossed: light skin tone */
	{ 0x1FAF0, 0x1F3FC }, /* hand with index finger and thumb crossed: medium-light skin tone */
	{ 0x1FAF0, 0x1F3FD }, /* hand with index finger and thumb crossed: medium skin tone */
	{ 0x1FAF0, 0x1F3FE }, /* hand with index finger and thumb crossed: medium-dark skin tone */
	{ 0x1FAF0, 0x1F3FF }, /* hand with index finger and thumb crossed: dark skin tone */
	{ 0x1FAF1, 0x1F3FB }, /* rightwards hand: light skin tone */
	{ 0x1FAF1, 0x1F3FC }, /* rightwards hand: medium-light skin tone */
	{ 0x1FAF1, 0x1F3FD }, /* rightwards hand: medium skin tone */
	{ 0x1FAF1, 0x1F3FE }, /* rightwards hand: medium-dark skin tone */
	{ 0x1FAF1, 0x1F3FF }, /* rightwards hand: dark skin tone */
	{ 0x1FAF2, 0x1F3FB }, /* leftwards hand: light skin tone */
	{ 0x1FAF2, 0x1F3FC }, /* leftwards hand: medium-light skin tone */
	{ 0x1FAF2, 0x1F3FD }, /* leftwards hand: medium skin tone */
	{ 0x1FAF2, 0x1F3FE }, /* leftwards hand: medium-dark skin tone */
	{ 0x1FAF2, 0x1F3FF }, /* leftwards hand: dark skin tone */
	{ 0x1FAF3, 0x1F3FB }, /* palm down hand: light skin tone */
	{ 0x1FAF3, 0x1F3FC }, /* palm down hand: medium-light skin tone */
	{ 0x1FAF3, 0x1F3FD }, /* palm down hand: medium skin tone */
	{ 0x1FAF3, 0x1F3FE }, /* palm down hand: medium-dark skin tone */
	{ 0x1FAF3, 0x1F3FF }, /* palm down hand: dark skin tone */
	{ 0x1FAF4, 0x1F3FB }, /* palm up hand: light skin tone */
	{ 0x1FAF4, 0x1F3FC }, /* palm up hand: medium-light skin tone */
	{ 0x1FAF4, 0x1F3FD }, /* palm up hand: medium skin tone */
	{ 0x1FAF4, 0x1F3FE }, /* palm up hand: medium-dark skin tone */
	{ 0x1FAF4, 0x1F3FF }, /* palm up hand: dark skin tone */
	{ 0x1FAF5, 0x1F3FB }, /* index pointing at the viewer: light skin tone */
	{ 0x1FAF5, 0x1F3FC }, /* index pointing at the viewer: medium-light skin tone */
	{ 0x1FAF5, 0x1F3FD }, /* index pointing at the viewer: medium skin tone */
	{ 0x1FAF5, 0x1F3FE }, /* index pointing at the viewer: medium-dark skin tone */
	{ 0x1FAF5, 0x1F3FF }, /* index pointing at the viewer: dark skin tone */
	{ 0x1FAF6, 0x1F3FB }, /* heart hands: light skin tone */
	{ 0x1FAF6, 0x1F3FC }, /* heart hands: medium-light skin tone */
	{ 0x1FAF6, 0x1F3FD }, /* heart hands: medium skin tone */
	{ 0x1FAF6, 0x1F3FE }, /* heart hands: medium-dark skin tone */
	{ 0x1FAF6, 0x1F3FF }, /* heart hands: dark skin tone */
	{ 0x1FAF7, 0x1F3FB }, /* leftwards pushing hand: light skin tone */
	{ 0x1FAF7, 0x1F3FC }, /* leftwards pushing hand: medium-light skin tone */
	{ 0x1FAF7, 0x1F3FD }, /* leftwards pushing hand: medium skin tone */
	{ 0x1FAF7, 0x1F3FE }, /* leftwards pushing hand: medium-dark skin tone */
	{ 0x1FAF7, 0x1F3FF }, /* leftwards pushing hand: dark skin tone */
	{ 0x1FAF8, 0x1F3FB }, /* rightwards pushing hand: light skin tone */
	{ 0x1FAF8, 0x1F3FC }, /* rightwards pushing hand: medium-light skin tone */
	{ 0x1FAF8, 0x1F3FD }, /* rightwards pushing hand: medium skin tone */
	{ 0x1FAF8, 0x1F3FE }, /* rightwards pushing hand: medium-dark skin tone */
	{ 0x1FAF8, 0x1F3FF }, /* rightwards pushing hand: dark skin tone */
};

struct utf8_combined_first {
	struct utf8_data		 first;

	struct utf8_data		*second;
	u_int				 count;

	RB_ENTRY(utf8_combined_first)	 entry;
};

static int
utf8_combined_first_cmp(struct utf8_combined_first *uf1,
    struct utf8_combined_first *uf2)
{
	struct utf8_data	*ud1 = &uf1->first, *ud2 = &uf2->first;

	if (ud1->size < ud2->size)
		return (-1);
	if (ud1->size > ud2->size)
		return (1);
	return (memcmp(ud1->data, ud2->data, sizeof *ud1->data));
}
RB_HEAD(utf8_combined_tree, utf8_combined_first);
RB_GENERATE_STATIC(utf8_combined_tree, utf8_combined_first, entry,
    utf8_combined_first_cmp);
static struct utf8_combined_tree utf8_combined_tree =
    RB_INITIALIZER(utf8_combined_tree);

static int
utf8_combined_second_cmp(const void *vp1, const void *vp2)
{
	const struct utf8_data	*ud1 = vp1, *ud2 = vp2;

	if (ud1->size < ud2->size)
		return (-1);
	if (ud1->size > ud2->size)
		return (1);
	return (memcmp(ud1->data, ud2->data, sizeof *ud1->data));
}

static int
utf8_is_zwj(const struct utf8_data *ud)
{
	return (ud->size == 3 && memcmp(ud->data, "\342\200\215", 3) == 0);
}

static struct utf8_data *
utf8_add_zwj(const struct utf8_data *ud)
{
	static struct utf8_data	new;

	if (ud->size + 3 > UTF8_SIZE)
		return (NULL);
	memset(&new, 0, sizeof new);
	memcpy(new.data, "\342\200\215", 3);
	memcpy(new.data + 3, ud->data, ud->size);
	new.size = 3 + ud->size;
	new.width = ud->width;
	return (&new);
}

static struct utf8_combined_first *
utf8_find_combined_first(const struct utf8_data *first)
{
	struct utf8_combined_first	uf;

	memset(&uf, 0, sizeof uf);
	utf8_copy(&uf.first, first);
	return (RB_FIND(utf8_combined_tree, &utf8_combined_tree, &uf));
}

static int
utf8_find_combined_second(struct utf8_combined_first *uf,
    const struct utf8_data *second)
{
	return (bsearch(second, uf->second, uf->count, sizeof *uf->second,
	    utf8_combined_second_cmp) != NULL);
}

void
utf8_build_combined(void)
{
	struct utf8_data		 first, second;
	int				 mlen;
	u_int				 i;
	wchar_t				 wc;
	struct utf8_combined_first	*uf;

	for (i = 0; i < nitems(utf8_combined_table); i++) {
		memset(&first, 0, sizeof first);
		wc = utf8_combined_table[i].first;
		mlen = wctomb(first.data, wc);
		if (mlen <= 0 || mlen > UTF8_SIZE) {
			log_debug("invalid combined character %08X", wc);
			continue;
		}
		first.size = mlen;

		uf = utf8_find_combined_first(&first);
		if (uf == NULL) {
			uf = xcalloc(1, sizeof *uf);
			utf8_copy(&uf->first, &first);
			RB_INSERT(utf8_combined_tree, &utf8_combined_tree, uf);
		}

		memset(&second, 0, sizeof second);
		wc = utf8_combined_table[i].second;
		mlen = wctomb(second.data, wc);
		if (mlen <= 0 || mlen > UTF8_SIZE) {
			log_debug("invalid combined character %05X", wc);
			continue;
		}
		second.size = mlen;

		log_debug("combined character %05X+%05X = %.*s+%.*s",
		    utf8_combined_table[i].first, utf8_combined_table[i].second,
		    (int)first.size, first.data, (int)second.size, second.data);

		uf->second = xreallocarray(uf->second, uf->count + 1,
		    sizeof *uf->second);
		utf8_copy(&uf->second[uf->count], &second);
		uf->count++;
	}

	RB_FOREACH(uf, utf8_combined_tree, &utf8_combined_tree) {
		qsort(uf->second, uf->count, sizeof *uf->second,
		    utf8_combined_second_cmp);
	}
}

int
utf8_try_combined(const struct utf8_data *ud, const struct utf8_data *last,
    const struct utf8_data **combine, u_int *width)
{
	struct utf8_combined_first	*uf;

	/* Use the incoming width by default. */
	*width = ud->width;

	/*
	 * If this is a zero width joiner, discard it but try to combine the
	 * next character.
	 */
	if (utf8_is_zwj(ud))
		return (UTF8_DISCARD_MAYBE_COMBINE);

	/*
	 * If there is a previous character to combine and it is a ZWJ,
	 * combine with the new character and a ZWJ.
	 */
	if (last != NULL && utf8_is_zwj(last)) {
		*combine = utf8_add_zwj(ud);
		if (*combine == NULL)
			return (UTF8_DISCARD_NOW);
		return (UTF8_COMBINE_NOW);
	}

	/*
	 * If the width of this character is zero, combine onto the previous
	 * character.
	 */
	if (ud->width == 0) {
		*combine = ud;
		return (UTF8_COMBINE_NOW);
	}

	/*
	 * Look up the character in the first character list, if it is missing,
	 * write it immediately. If it is present, write but try to combine
	 * later; also force the width to two.
	 */
	if (last == NULL) {
		if (utf8_find_combined_first(ud) != NULL) {
			*width = 2;
			return (UTF8_WRITE_MAYBE_COMBINE);
		}
		return (UTF8_WRITE_NOW);
	}

	/*
	 * This must be a potential combined character. If both first and
	 * second characters are on the list, combine.
	 */
	uf = utf8_find_combined_first(last);
	if (uf != NULL && utf8_find_combined_second(uf, ud)) {
		*combine = ud;
		return (UTF8_COMBINE_NOW);
	}

	return (UTF8_WRITE_NOW);
}
