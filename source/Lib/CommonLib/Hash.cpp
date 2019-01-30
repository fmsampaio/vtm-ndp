/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2019, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /** \file     TEncHash.cpp
     \brief    hash encoder class
 */
#include "CommonLib/dtrace_codingstruct.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"
#include "Hash.h"



 // ====================================================================================================================
 // Constructor / destructor / create / destroy
 // ====================================================================================================================

int TComHash::m_blockSizeToIndex[65][65];
TCRCCalculatorLight TComHash::m_crcCalculator1(24, 0x5D6DCB);
TCRCCalculatorLight TComHash::m_crcCalculator2(24, 0x864CFB);

TCRCCalculatorLight::TCRCCalculatorLight(unsigned int bits, unsigned int truncPoly)
{
  m_remainder = 0;
  m_bits = bits;
  m_truncPoly = truncPoly;
  m_finalResultMask = (1 << bits) - 1;

  xInitTable();
}

TCRCCalculatorLight::~TCRCCalculatorLight()
{

}

void TCRCCalculatorLight::xInitTable()
{
  const unsigned int highBit = 1 << (m_bits - 1);
  const unsigned int ByteHighBit = 1 << (8 - 1);

  for (unsigned int value = 0; value < 256; value++)
  {
    unsigned int remainder = 0;
    for (unsigned char mask = ByteHighBit; mask != 0; mask >>= 1)
    {
      if (value & mask)
      {
        remainder ^= highBit;
      }

      if (remainder & highBit)
      {
        remainder <<= 1;
        remainder ^= m_truncPoly;
      }
      else
      {
        remainder <<= 1;
      }
    }

    m_table[value] = remainder;
  }
}

void TCRCCalculatorLight::processData(unsigned char* curData, unsigned int dataLength)
{
  for (unsigned int i = 0; i < dataLength; i++)
  {
    unsigned char index = (m_remainder >> (m_bits - 8)) ^ curData[i];
    m_remainder <<= 8;
    m_remainder ^= m_table[index];
  }
}


TComHash::TComHash()
{
  m_lookupTable = NULL;
  tableHasContent = false;
}

TComHash::~TComHash()
{
  clearAll();
  if (m_lookupTable != NULL)
  {
    delete[] m_lookupTable;
    m_lookupTable = NULL;
  }
}

void TComHash::create()
{
  if (m_lookupTable != NULL)
  {
    clearAll();
    return;
  }
  int maxAddr = 1 << (m_CRCBits + m_blockSizeBits);
  m_lookupTable = new std::vector<BlockHash>*[maxAddr];
  memset(m_lookupTable, 0, sizeof(std::vector<BlockHash>*) * maxAddr);
  tableHasContent = false;
}

void TComHash::clearAll()
{
  tableHasContent = false;
  if (m_lookupTable == NULL)
  {
    return;
  }
  int maxAddr = 1 << (m_CRCBits + m_blockSizeBits);
  for (int i = 0; i < maxAddr; i++)
  {
    if (m_lookupTable[i] != NULL)
    {
      delete m_lookupTable[i];
      m_lookupTable[i] = NULL;
    }
  }
}

void TComHash::addToTable(unsigned int hashValue, const BlockHash& blockHash)
{
  if (m_lookupTable[hashValue] == NULL)
  {
    m_lookupTable[hashValue] = new std::vector<BlockHash>;
    m_lookupTable[hashValue]->push_back(blockHash);
  }
  else
  {
    m_lookupTable[hashValue]->push_back(blockHash);
  }
}

int TComHash::count(unsigned int hashValue)
{
  if (m_lookupTable[hashValue] == NULL)
  {
    return 0;
  }
  else
  {
    return static_cast<int>(m_lookupTable[hashValue]->size());
  }
}

int TComHash::count(unsigned int hashValue) const
{
  if (m_lookupTable[hashValue] == NULL)
  {
    return 0;
  }
  else
  {
    return static_cast<int>(m_lookupTable[hashValue]->size());
  }
}

MapIterator TComHash::getFirstIterator(unsigned int hashValue)
{
  return m_lookupTable[hashValue]->begin();
}

const MapIterator TComHash::getFirstIterator(unsigned int hashValue) const
{
  return m_lookupTable[hashValue]->begin();
}

bool TComHash::hasExactMatch(unsigned int hashValue1, unsigned int hashValue2)
{
  if (m_lookupTable[hashValue1] == NULL)
  {
    return false;
  }
  std::vector<BlockHash>::iterator it;
  for (it = m_lookupTable[hashValue1]->begin(); it != m_lookupTable[hashValue1]->end(); it++)
  {
    if ((*it).hashValue2 == hashValue2)
    {
      return true;
    }
  }
  return false;
}

void TComHash::generateBlock2x2HashValue(const PelUnitBuf &curPicBuf, int picWidth, int picHeight, const BitDepths bitDepths, unsigned int* picBlockHash[2], bool* picBlockSameInfo[3])
{
  const int width = 2;
  const int height = 2;
  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  int length = width * 2;
  bool includeChroma = false;
  if ((curPicBuf).chromaFormat == CHROMA_444)
  {
    length *= 3;
    includeChroma = true;
  }
  unsigned char* p = new unsigned char[length];

  int pos = 0;
  for (int yPos = 0; yPos < yEnd; yPos++)
  {
    for (int xPos = 0; xPos < xEnd; xPos++)
    {
      TComHash::getPixelsIn1DCharArrayByBlock2x2(curPicBuf, p, xPos, yPos, bitDepths, includeChroma);
      picBlockSameInfo[0][pos] = isBlock2x2RowSameValue(p, includeChroma);
      picBlockSameInfo[1][pos] = isBlock2x2ColSameValue(p, includeChroma);

      picBlockHash[0][pos] = TComHash::getCRCValue1(p, length * sizeof(unsigned char));
      picBlockHash[1][pos] = TComHash::getCRCValue2(p, length * sizeof(unsigned char));

      pos++;
    }
    pos += width - 1;
  }

  delete[] p;
}
void TComHash::generateRectangleHashValue(int picWidth, int picHeight, int width, int height, unsigned int* srcPicBlockHash[2], unsigned int* dstPicBlockHash[2], bool* srcPicBlockSameInfo[3], bool* dstPicBlockSameInfo[3])
{
  //at present, only support 1:2(2:1) retangle hash value
  CHECK(width != (height << 1) && (width << 1) != height, "Wrong")
  bool isHorizontal = width == (height << 1) ? true : false;

  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  int srcWidth = width >> 1;
  int quadWidth = width >> 2;
  int srcHeight = height >> 1;
  int quadHeight = height >> 2;

  int length = 2 * sizeof(unsigned int);
  unsigned int* p = new unsigned int[2];
  int pos = 0;
  if (isHorizontal)
  {
    for (int yPos = 0; yPos < yEnd; yPos++)
    {
      for (int xPos = 0; xPos < xEnd; xPos++)
      {
        p[0] = srcPicBlockHash[0][pos];
        p[1] = srcPicBlockHash[0][pos + srcWidth];
        dstPicBlockHash[0][pos] = TComHash::getCRCValue1((unsigned char*)p, length);

        p[0] = srcPicBlockHash[1][pos];
        p[1] = srcPicBlockHash[1][pos + srcWidth];
        dstPicBlockHash[1][pos] = TComHash::getCRCValue2((unsigned char*)p, length);

        dstPicBlockSameInfo[0][pos] = srcPicBlockSameInfo[0][pos] && srcPicBlockSameInfo[0][pos + quadWidth] && srcPicBlockSameInfo[0][pos + srcWidth];
        dstPicBlockSameInfo[1][pos] = srcPicBlockSameInfo[1][pos] && srcPicBlockSameInfo[1][pos + srcWidth];
        pos++;
      }
      pos += width - 1;
    }
  }
  else
  {
    for (int yPos = 0; yPos < yEnd; yPos++)
    {
      for (int xPos = 0; xPos < xEnd; xPos++)
      {
        p[0] = srcPicBlockHash[0][pos];
        p[1] = srcPicBlockHash[0][pos + srcHeight * picWidth];
        dstPicBlockHash[0][pos] = TComHash::getCRCValue1((unsigned char*)p, length);

        p[0] = srcPicBlockHash[1][pos];
        p[1] = srcPicBlockHash[1][pos + srcHeight * picWidth];
        dstPicBlockHash[1][pos] = TComHash::getCRCValue2((unsigned char*)p, length);

        dstPicBlockSameInfo[0][pos] = srcPicBlockSameInfo[0][pos] && srcPicBlockSameInfo[0][pos + srcHeight * picWidth];
        dstPicBlockSameInfo[1][pos] = srcPicBlockSameInfo[1][pos] && srcPicBlockSameInfo[1][pos + quadHeight * picWidth] && srcPicBlockSameInfo[1][pos + srcHeight * picWidth];

        pos++;
      }
      pos += width - 1;
    }
  }

  int widthMinus1 = width - 1;
  int heightMinus1 = height - 1;
  pos = 0;

  for (int yPos = 0; yPos < yEnd; yPos++)
  {
    for (int xPos = 0; xPos < xEnd; xPos++)
    {
      dstPicBlockSameInfo[2][pos] = (!dstPicBlockSameInfo[0][pos] && !dstPicBlockSameInfo[1][pos]) || (((xPos & widthMinus1) == 0) && ((yPos & heightMinus1) == 0));
      pos++;
    }
    pos += width - 1;
  }

  delete[] p;
}

void TComHash::generateBlockHashValue(int picWidth, int picHeight, int width, int height, unsigned int* srcPicBlockHash[2], unsigned int* dstPicBlockHash[2], bool* srcPicBlockSameInfo[3], bool* dstPicBlockSameInfo[3])
{
  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  int srcWidth = width >> 1;
  int quadWidth = width >> 2;
  int srcHeight = height >> 1;
  int quadHeight = height >> 2;

  int length = 4 * sizeof(unsigned int);

  unsigned int* p = new unsigned int[4];
  int pos = 0;
  for (int yPos = 0; yPos < yEnd; yPos++)
  {
    for (int xPos = 0; xPos < xEnd; xPos++)
    {
      p[0] = srcPicBlockHash[0][pos];
      p[1] = srcPicBlockHash[0][pos + srcWidth];
      p[2] = srcPicBlockHash[0][pos + srcHeight * picWidth];
      p[3] = srcPicBlockHash[0][pos + srcHeight * picWidth + srcWidth];
      dstPicBlockHash[0][pos] = TComHash::getCRCValue1((unsigned char*)p, length);

      p[0] = srcPicBlockHash[1][pos];
      p[1] = srcPicBlockHash[1][pos + srcWidth];
      p[2] = srcPicBlockHash[1][pos + srcHeight * picWidth];
      p[3] = srcPicBlockHash[1][pos + srcHeight * picWidth + srcWidth];
      dstPicBlockHash[1][pos] = TComHash::getCRCValue2((unsigned char*)p, length);

      dstPicBlockSameInfo[0][pos] = srcPicBlockSameInfo[0][pos] && srcPicBlockSameInfo[0][pos + quadWidth] && srcPicBlockSameInfo[0][pos + srcWidth]
        && srcPicBlockSameInfo[0][pos + srcHeight * picWidth] && srcPicBlockSameInfo[0][pos + srcHeight * picWidth + quadWidth] && srcPicBlockSameInfo[0][pos + srcHeight * picWidth + srcWidth];

      dstPicBlockSameInfo[1][pos] = srcPicBlockSameInfo[1][pos] && srcPicBlockSameInfo[1][pos + srcWidth] && srcPicBlockSameInfo[1][pos + quadHeight * picWidth]
        && srcPicBlockSameInfo[1][pos + quadHeight * picWidth + srcWidth] && srcPicBlockSameInfo[1][pos + srcHeight * picWidth] && srcPicBlockSameInfo[1][pos + srcHeight * picWidth + srcWidth];

      pos++;
    }
    pos += width - 1;
  }

  if (width >= 4)
  {
    int widthMinus1 = width - 1;
    int heightMinus1 = height - 1;
    pos = 0;

    for (int yPos = 0; yPos < yEnd; yPos++)
    {
      for (int xPos = 0; xPos < xEnd; xPos++)
      {
        dstPicBlockSameInfo[2][pos] = (!dstPicBlockSameInfo[0][pos] && !dstPicBlockSameInfo[1][pos]) || (((xPos & widthMinus1) == 0) && ((yPos & heightMinus1) == 0));
        pos++;
      }
      pos += width - 1;
    }
  }

  delete[] p;

}

void TComHash::addToHashMapByRowWithPrecalData(unsigned int* picHash[2], bool* picIsSame, int picWidth, int picHeight, int width, int height)
{
  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  bool* srcIsAdded = picIsSame;
  unsigned int* srcHash[2] = { picHash[0], picHash[1] };

  int addValue = m_blockSizeToIndex[width][height];
  CHECK(addValue < 0, "Wrong")
    addValue <<= m_CRCBits;
  int crcMask = 1 << m_CRCBits;
  crcMask -= 1;

  for (int xPos = 0; xPos < xEnd; xPos++)
  {
    for (int yPos = 0; yPos < yEnd; yPos++)
    {
      int pos = yPos * picWidth + xPos;
      //valid data
      if (srcIsAdded[pos])
      {
        BlockHash blockHash;
        blockHash.x = xPos;
        blockHash.y = yPos;

        unsigned int      hashValue1 = (srcHash[0][pos] & crcMask) + addValue;
        blockHash.hashValue2 = srcHash[1][pos];

        addToTable(hashValue1, blockHash);
      }
    }
  }
}

void TComHash::getPixelsIn1DCharArrayByBlock2x2(const PelUnitBuf &curPicBuf, unsigned char* pixelsIn1D, int xStart, int yStart, const BitDepths& bitDepths, bool includeAllComponent)
{
  ChromaFormat fmt = (curPicBuf).chromaFormat;
  if (fmt != CHROMA_444)
  {
    includeAllComponent = false;
  }

  if (bitDepths.recon[CHANNEL_TYPE_LUMA] == 8 && bitDepths.recon[CHANNEL_TYPE_CHROMA] == 8)
  {
    Pel* curPel[3];
    int stride[3];
    for (int id = 0; id < 3; id++)
    {
      ComponentID compID = ComponentID(id);
      stride[id] = (curPicBuf).get(compID).stride;
      curPel[id] = (curPicBuf).get(compID).buf;
      curPel[id] += (yStart >> getComponentScaleY(compID, fmt)) * stride[id] + (xStart >> getComponentScaleX(compID, fmt));
    }

    int index = 0;
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        pixelsIn1D[index++] = static_cast<unsigned char>(curPel[0][j]);
        if (includeAllComponent)
        {
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[1][j]);
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[2][j]);
        }
      }
      curPel[0] += stride[0];
      if (includeAllComponent)
      {
        curPel[1] += stride[1];
        curPel[2] += stride[2];
      }
    }
  }
  else
  {
    int shift = bitDepths.recon[CHANNEL_TYPE_LUMA] - 8;
    int shiftc = bitDepths.recon[CHANNEL_TYPE_CHROMA] - 8;
    Pel* curPel[3];
    int stride[3];
    for (int id = 0; id < 3; id++)
    {
      ComponentID compID = ComponentID(id);
      stride[id] = (curPicBuf).get(compID).stride;
      curPel[id] = (curPicBuf).get(compID).buf;
      curPel[id] += (yStart >> getComponentScaleY(compID, fmt)) * stride[id] + (xStart >> getComponentScaleX(compID, fmt));
    }

    int index = 0;
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        pixelsIn1D[index++] = static_cast<unsigned char>(curPel[0][j] >> shift);
        if (includeAllComponent)
        {
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[1][j] >> shiftc);
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[2][j] >> shiftc);
        }
      }
      curPel[0] += stride[0];
      if (includeAllComponent)
      {
        curPel[1] += stride[1];
        curPel[2] += stride[2];
      }
    }
  }
}

bool TComHash::isBlock2x2RowSameValue(unsigned char* p, bool includeAllComponent)
{
  if (includeAllComponent)
  {
    if (p[0] != p[3] || p[6] != p[9])
    {
      return false;
    }
    if (p[1] != p[4] || p[7] != p[10])
    {
      return false;
    }
    if (p[2] != p[5] || p[8] != p[11])
    {
      return false;
    }
  }
  else
  {
    if (p[0] != p[1] || p[2] != p[3])
    {
      return false;
    }
  }

  return true;
}

bool TComHash::isBlock2x2ColSameValue(unsigned char* p, bool includeAllComponent)
{
  if (includeAllComponent)
  {
    if (p[0] != p[6] || p[3] != p[9])
    {
      return false;
    }
    if (p[1] != p[7] || p[4] != p[10])
    {
      return false;
    }
    if (p[2] != p[8] || p[5] != p[11])
    {
      return false;
    }
  }
  else
  {
    if ((p[0] != p[2]) || (p[1] != p[3]))
    {
      return false;
    }
  }

  return true;
}

bool TComHash::getBlockHashValue(const PelUnitBuf &curPicBuf, int width, int height, int xStart, int yStart, const BitDepths bitDepths, unsigned int& hashValue1, unsigned int& hashValue2)
{
  int addValue = m_blockSizeToIndex[width][height];

  CHECK(addValue < 0, "Wrong")
  addValue <<= m_CRCBits;
  int crcMask = 1 << m_CRCBits;
  crcMask -= 1;
  int length = 4;
  bool includeChroma = false;
  if ((curPicBuf).chromaFormat == CHROMA_444)
  {
    length *= 3;
    includeChroma = true;
  }

  unsigned char* p = new unsigned char[length];
  unsigned int* toHash = new unsigned int[4];

  int block2x2Num = (width*height) >> 2;

  unsigned int* hashValueBuffer[2][2];
  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      hashValueBuffer[i][j] = new unsigned int[block2x2Num];
    }
  }

  //2x2 subblock hash values in current CU
  int subBlockInWidth = (width >> 1);
  int subBlockInHeight = (height >> 1);
  for (int yPos = 0; yPos < height; yPos += 2)
  {
    for (int xPos = 0; xPos < width; xPos += 2)
    {
      int pos = (yPos >> 1)*subBlockInWidth + (xPos >> 1);
      TComHash::getPixelsIn1DCharArrayByBlock2x2(curPicBuf, p, xStart + xPos, yStart + yPos, bitDepths, includeChroma);

      hashValueBuffer[0][0][pos] = TComHash::getCRCValue1(p, length * sizeof(unsigned char));
      hashValueBuffer[1][0][pos] = TComHash::getCRCValue2(p, length * sizeof(unsigned char));
    }
  }

  int srcSubBlockInWidth = subBlockInWidth;
  subBlockInWidth >>= 1;
  subBlockInHeight >>= 1;
  length = 4 * sizeof(unsigned int);

  int srcIdx = 1;
  int dstIdx = 0;

  //4x4 subblock hash values to current block hash values
  int minSize = std::min(height, width);
  for (int subWidth = 4; subWidth <= minSize; subWidth *= 2)
  {
    srcIdx = 1 - srcIdx;
    dstIdx = 1 - dstIdx;

    int dstPos = 0;
    for (int yPos = 0; yPos < subBlockInHeight; yPos++)
    {
      for (int xPos = 0; xPos < subBlockInWidth; xPos++)
      {
        int srcPos = (yPos << 1)*srcSubBlockInWidth + (xPos << 1);

        toHash[0] = hashValueBuffer[0][srcIdx][srcPos];
        toHash[1] = hashValueBuffer[0][srcIdx][srcPos + 1];
        toHash[2] = hashValueBuffer[0][srcIdx][srcPos + srcSubBlockInWidth];
        toHash[3] = hashValueBuffer[0][srcIdx][srcPos + srcSubBlockInWidth + 1];

        hashValueBuffer[0][dstIdx][dstPos] = TComHash::getCRCValue1((unsigned char*)toHash, length);

        toHash[0] = hashValueBuffer[1][srcIdx][srcPos];
        toHash[1] = hashValueBuffer[1][srcIdx][srcPos + 1];
        toHash[2] = hashValueBuffer[1][srcIdx][srcPos + srcSubBlockInWidth];
        toHash[3] = hashValueBuffer[1][srcIdx][srcPos + srcSubBlockInWidth + 1];
        hashValueBuffer[1][dstIdx][dstPos] = TComHash::getCRCValue2((unsigned char*)toHash, length);

        dstPos++;
      }
    }

    srcSubBlockInWidth = subBlockInWidth;
    subBlockInWidth >>= 1;
    subBlockInHeight >>= 1;
  }

  if (width != height)//currently support 1:2 or 2:1 block size
  {
    CHECK(width != (height << 1) && (width << 1) != height, "Wrong")
      bool isHorizontal = width == (height << 1) ? true : false;
    length = 2 * sizeof(unsigned int);
    srcIdx = 1 - srcIdx;
    dstIdx = 1 - dstIdx;
    if (isHorizontal)
    {
      toHash[0] = hashValueBuffer[0][srcIdx][0];
      toHash[1] = hashValueBuffer[0][srcIdx][1];

      hashValueBuffer[0][dstIdx][0] = TComHash::getCRCValue1((unsigned char*)toHash, length);

      toHash[0] = hashValueBuffer[1][srcIdx][0];
      toHash[1] = hashValueBuffer[1][srcIdx][1];
      hashValueBuffer[1][dstIdx][0] = TComHash::getCRCValue2((unsigned char*)toHash, length);
    }
    else
    {
      CHECK(srcSubBlockInWidth != 1, "Wrong")
      toHash[0] = hashValueBuffer[0][srcIdx][0];
      toHash[1] = hashValueBuffer[0][srcIdx][srcSubBlockInWidth];

      hashValueBuffer[0][dstIdx][0] = TComHash::getCRCValue1((unsigned char*)toHash, length);

      toHash[0] = hashValueBuffer[1][srcIdx][0];
      toHash[1] = hashValueBuffer[1][srcIdx][srcSubBlockInWidth];
      hashValueBuffer[1][dstIdx][0] = TComHash::getCRCValue2((unsigned char*)toHash, length);
    }
  }

  hashValue1 = (hashValueBuffer[0][dstIdx][0] & crcMask) + addValue;
  hashValue2 = hashValueBuffer[1][dstIdx][0];

  delete[] toHash;

  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      delete[] hashValueBuffer[i][j];
    }
  }

  delete[] p;

  return true;
}

void TComHash::initBlockSizeToIndex()
{
  for (int i = 0; i < 65; i++)
  {
    for (int j = 0; j < 65; j++)
    {
      m_blockSizeToIndex[i][j] = -1;
    }
  }

  m_blockSizeToIndex[8][8] = 0;
  m_blockSizeToIndex[16][16] = 1;
  m_blockSizeToIndex[32][32] = 2;
  m_blockSizeToIndex[64][64] = 3;
  m_blockSizeToIndex[4][4] = 4;
  m_blockSizeToIndex[4][8] = 5;
  m_blockSizeToIndex[8][4] = 6;
}

unsigned int TComHash::getCRCValue1(unsigned char* p, int length)
{
  m_crcCalculator1.reset();
  m_crcCalculator1.processData(p, length);
  return m_crcCalculator1.getCRC();
}

unsigned int TComHash::getCRCValue2(unsigned char* p, int length)
{
  m_crcCalculator2.reset();
  m_crcCalculator2.processData(p, length);
  return m_crcCalculator2.getCRC();
}
//! \}
