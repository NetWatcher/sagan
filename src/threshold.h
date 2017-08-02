/*
** Copyright (C) 2009-2017 Quadrant Information Security <quadrantsec.com>
** Copyright (C) 2009-2017 Champ Clark III <cclark@quadrantsec.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

sbool Thresh_By_Src ( int rule_position, char *ip_src, unsigned char *ip_src_bits, char *selector );
sbool Thresh_By_Dst ( int rule_position, char *ip_dst, unsigned char *ip_dst_bits, char *selector );
sbool Thresh_By_Username( int rule_position, char *normalize_username, char *selector );
sbool Thresh_By_SrcPort( int rule_position, uint32_t ip_srcport_u32, char *selector );
sbool Thresh_By_DstPort( int rule_position, uint32_t ip_dstport_u32, char *selector );

