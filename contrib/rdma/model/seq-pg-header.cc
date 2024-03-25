/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "seq-pg-header.h"

NS_LOG_COMPONENT_DEFINE ("SeqPgHeader");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SeqPgHeader);

SeqPgHeader::SeqPgHeader ()
  : m_seq (0)
{
}

void
SeqPgHeader::SetSeq (uint32_t seq)
{
  m_seq = seq;
}
uint32_t
SeqPgHeader::GetSeq (void) const
{
  return m_seq;
}

void
SeqPgHeader::SetPG (uint16_t pg)
{
	m_pg = pg;
}
uint16_t
SeqPgHeader::GetPG (void) const
{
	return m_pg;
}

TypeId
SeqPgHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SeqPgHeader")
    .SetParent<Header> ()
    .AddConstructor<SeqPgHeader> ()
  ;
  return tid;
}
TypeId
SeqPgHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void
SeqPgHeader::Print (std::ostream &os) const
{
  //os << "(seq=" << m_seq << " time=" << TimeStep (m_ts).GetSeconds () << ")";
	//os << m_seq << " " << TimeStep (m_ts).GetSeconds () << " " << m_pg;
	os << m_seq << " " << m_pg;
}
uint32_t
SeqPgHeader::GetSerializedSize (void) const
{
	return GetHeaderSize();
}
uint32_t SeqPgHeader::GetHeaderSize(void){
	return 6;
}

void
SeqPgHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_seq);
  i.WriteHtonU16 (m_pg);
}
uint32_t
SeqPgHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_seq = i.ReadNtohU32 ();
  m_pg =  i.ReadNtohU16 ();

  // read IntHeader
  return GetSerializedSize ();
}

} // namespace ns3
