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

/** \file     ContextModelling.h
 *  \brief    Classes providing probability descriptions and contexts (header)
 */

#ifndef __CONTEXTMODELLING__
#define __CONTEXTMODELLING__


#include "CommonDef.h"
#include "Contexts.h"
#include "Slice.h"
#include "Unit.h"
#include "UnitPartitioner.h"

#include <bitset>


struct CoeffCodingContext
{
public:
  CoeffCodingContext( const TransformUnit& tu, ComponentID component, bool signHide, bool bdpcm = false );
public:
  void  initSubblock     ( int SubsetId, bool sigGroupFlag = false );
public:
  void  resetSigGroup   ()                      { m_sigCoeffGroupFlag.reset( m_subSetPos ); }
  void  setSigGroup     ()                      { m_sigCoeffGroupFlag.set( m_subSetPos ); }
  bool  noneSigGroup    ()                      { return m_sigCoeffGroupFlag.none(); }
  int   lastSubSet      ()                      { return ( maxNumCoeff() - 1 ) >> log2CGSize(); }
  bool  isLastSubSet    ()                      { return lastSubSet() == m_subSetId; }
  bool  only1stSigGroup ()                      { return m_sigCoeffGroupFlag.count()-m_sigCoeffGroupFlag[lastSubSet()]==0; }
  void  setScanPosLast  ( int       posLast )   { m_scanPosLast = posLast; }
public:
  ComponentID     compID          ()                        const { return m_compID; }
  int             subSetId        ()                        const { return m_subSetId; }
  int             subSetPos       ()                        const { return m_subSetPos; }
  int             cgPosY          ()                        const { return m_subSetPosY; }
  int             cgPosX          ()                        const { return m_subSetPosX; }
  unsigned        width           ()                        const { return m_width; }
  unsigned        height          ()                        const { return m_height; }
  unsigned        log2CGWidth     ()                        const { return m_log2CGWidth; }
  unsigned        log2CGHeight    ()                        const { return m_log2CGHeight; }
  unsigned        log2CGSize      ()                        const { return m_log2CGSize; }
  bool            extPrec         ()                        const { return m_extendedPrecision; }
  int             maxLog2TrDRange ()                        const { return m_maxLog2TrDynamicRange; }
  unsigned        maxNumCoeff     ()                        const { return m_maxNumCoeff; }
  int             scanPosLast     ()                        const { return m_scanPosLast; }
  int             minSubPos       ()                        const { return m_minSubPos; }
  int             maxSubPos       ()                        const { return m_maxSubPos; }
  bool            isLast          ()                        const { return ( ( m_scanPosLast >> m_log2CGSize ) == m_subSetId ); }
  bool            isNotFirst      ()                        const { return ( m_subSetId != 0 ); }
  bool            isSigGroup(int scanPosCG) const { return m_sigCoeffGroupFlag[m_scanCG[scanPosCG].idx]; }
  bool            isSigGroup      ()                        const { return m_sigCoeffGroupFlag[ m_subSetPos ]; }
  bool            signHiding      ()                        const { return m_signHiding; }
  bool            hideSign        ( int       posFirst,
                                    int       posLast   )   const { return ( m_signHiding && ( posLast - posFirst >= SBH_THRESHOLD ) ); }
  CoeffScanType   scanType        ()                        const { return m_scanType; }
  unsigned        blockPos(int scanPos) const { return m_scan[scanPos].idx; }
  unsigned        posX(int scanPos) const { return m_scan[scanPos].x; }
  unsigned        posY(int scanPos) const { return m_scan[scanPos].y; }
  unsigned        maxLastPosX     ()                        const { return m_maxLastPosX; }
  unsigned        maxLastPosY     ()                        const { return m_maxLastPosY; }
  unsigned        lastXCtxId      ( unsigned  posLastX  )   const { return m_CtxSetLastX( m_lastOffsetX + ( posLastX >> m_lastShiftX ) ); }
  unsigned        lastYCtxId      ( unsigned  posLastY  )   const { return m_CtxSetLastY( m_lastOffsetY + ( posLastY >> m_lastShiftY ) ); }
  bool            isContextCoded  ()                              { return --m_remainingContextBins >= 0; }
  int             numCtxBins      ()                        const { return   m_remainingContextBins;      }
  void            setNumCtxBins   ( int n )                       {          m_remainingContextBins  = n; }
  unsigned        sigGroupCtxId   ( bool ts = false     )   const { return ts ? m_sigGroupCtxIdTS : m_sigGroupCtxId; }
  bool            bdpcm           ()                        const { return m_bdpcm; }
  unsigned sigCtxIdAbs( int scanPos, const TCoeff* coeff, const int state )
  {
    const uint32_t posY      = m_scan[scanPos].y;
    const uint32_t posX      = m_scan[scanPos].x;
    const TCoeff* pData     = coeff + posX + posY * m_width;
    const int     diag      = posX + posY;
    int           numPos    = 0;
    int           sumAbs    = 0;
#define UPDATE(x) {int a=abs(x);sumAbs+=std::min(4+(a&1),a);numPos+=!!a;}
    if( posX < m_width-1 )
    {
      UPDATE( pData[1] );
      if( posX < m_width-2 )
      {
        UPDATE( pData[2] );
      }
      if( posY < m_height-1 )
      {
        UPDATE( pData[m_width+1] );
      }
    }
    if( posY < m_height-1 )
    {
      UPDATE( pData[m_width] );
      if( posY < m_height-2 )
      {
        UPDATE( pData[m_width<<1] );
      }
    }
#undef UPDATE
    int ctxOfs = std::min( sumAbs, 5 ) + ( diag < 2 ? 6 : 0 );
    if( m_chType == CHANNEL_TYPE_LUMA )
    {
      ctxOfs += diag < 5 ? 6 : 0;
    }
    m_tmplCpDiag = diag;
    m_tmplCpSum1 = sumAbs - numPos;
    return m_sigFlagCtxSet[std::max( 0, state-1 )]( ctxOfs );
  }

  uint8_t ctxOffsetAbs()
  {
    int offset = 0;
    if( m_tmplCpDiag != -1 )
    {
      offset  = std::min( m_tmplCpSum1, 4 ) + 1;
      offset += ( !m_tmplCpDiag ? ( m_chType == CHANNEL_TYPE_LUMA ? 15 : 5 ) : m_chType == CHANNEL_TYPE_LUMA ? m_tmplCpDiag < 3 ? 10 : ( m_tmplCpDiag < 10 ? 5 : 0 ) : 0 );
    }
    return uint8_t(offset);
  }

  unsigned parityCtxIdAbs   ( uint8_t offset )  const { return m_parFlagCtxSet   ( offset ); }
  unsigned greater1CtxIdAbs ( uint8_t offset )  const { return m_gtxFlagCtxSet[1]( offset ); }
  unsigned greater2CtxIdAbs ( uint8_t offset )  const { return m_gtxFlagCtxSet[0]( offset ); }
  unsigned templateAbsSum( int scanPos, const TCoeff* coeff, int baseLevel )
  {
    const uint32_t  posY  = m_scan[scanPos].y;
    const uint32_t  posX  = m_scan[scanPos].x;
    const TCoeff*   pData = coeff + posX + posY * m_width;
    int             sum   = 0;
    if (posX < m_width - 1)
    {
      sum += abs(pData[1]);
      if (posX < m_width - 2)
      {
        sum += abs(pData[2]);
      }
      if (posY < m_height - 1)
      {
        sum += abs(pData[m_width + 1]);
      }
    }
    if (posY < m_height - 1)
    {
      sum += abs(pData[m_width]);
      if (posY < m_height - 2)
      {
        sum += abs(pData[m_width << 1]);
      }
    }
    return std::max(std::min(sum - 5 * baseLevel, 31), 0);
  }

  unsigned sigCtxIdAbsTS( int scanPos, const TCoeff* coeff )
  {
    const uint32_t  posY   = m_scan[scanPos].y;
    const uint32_t  posX   = m_scan[scanPos].x;
    const TCoeff*   posC   = coeff + posX + posY * m_width;
    int             numPos = 0;
#define UPDATE(x) {int a=abs(x);numPos+=!!a;}
    if( posX > 0 )
    {
      UPDATE( posC[-1] );
    }
    if( posY > 0 )
    {
      UPDATE( posC[-(int)m_width] );
    }
#undef UPDATE

    return m_tsSigFlagCtxSet( numPos );
  }

  unsigned parityCtxIdAbsTS   ()                  const { return m_tsParFlagCtxSet(      0 ); }
  unsigned greaterXCtxIdAbsTS ( uint8_t offset )  const { return m_tsGtxFlagCtxSet( offset ); }

#if JVET_O0122_TS_SIGN_LEVEL
  unsigned lrg1CtxIdAbsTS(int scanPos, const TCoeff* coeff, int bdpcm)
  {
    const uint32_t  posY = m_scan[scanPos].y;
    const uint32_t  posX = m_scan[scanPos].x;
    const TCoeff*   posC = coeff + posX + posY * m_width;

    int             numPos = 0;
#define UPDATE(x) {int a=abs(x);numPos+=!!a;}

    if (bdpcm)
    {
      numPos = 3;
    }
    else
    {
      if (posX > 0)
      {
        UPDATE(posC[-1]);
      }
      if (posY > 0)
      {
        UPDATE(posC[-(int)m_width]);
      }
    }

#undef UPDATE
    return m_tsLrg1FlagCtxSet(numPos);
  }
#endif

#if JVET_O0122_TS_SIGN_LEVEL
  unsigned signCtxIdAbsTS(int scanPos, const TCoeff* coeff, int bdpcm)
  {
    const uint32_t  posY = m_scan[scanPos].y;
    const uint32_t  posX = m_scan[scanPos].x;
    const TCoeff*   pData = coeff + posX + posY * m_width;

    int rightSign = 0, belowSign = 0;
    unsigned signCtx = 0;

    if (posX > 0)
    {
      rightSign = pData[-1];
    }
    if (posY > 0)
    {
      belowSign = pData[-(int)m_width];
    }

    if ((rightSign == 0 && belowSign == 0) || ((rightSign*belowSign) < 0))
    {
      signCtx = 0;
    }
    else if (rightSign >= 0 && belowSign >= 0)
    {
      signCtx = 1;
    }
    else
    {
      signCtx = 2;
    }
    if (bdpcm)
    {
      signCtx += 3;
    }
    return m_tsSignFlagCtxSet(signCtx);
  }
#endif

#if JVET_O0122_TS_SIGN_LEVEL
  void neighTS(int &rightPixel, int &belowPixel, int scanPos, const TCoeff* coeff)
  {
    const uint32_t  posY = m_scan[scanPos].y;
    const uint32_t  posX = m_scan[scanPos].x;
    const TCoeff*   data = coeff + posX + posY * m_width;

    rightPixel = belowPixel = 0;

    if (posX > 0)
    {
      rightPixel = data[-1];
    }
    if (posY > 0)
    {
      belowPixel = data[-(int)m_width];
    }
  }

  int deriveModCoeff(int rightPixel, int belowPixel, int absCoeff, int bdpcm = 0)
  {
    int pred1, absBelow = abs(belowPixel), absRight = abs(rightPixel);

    int absCoeffMod = absCoeff;

    if (bdpcm == 0)
    {
      pred1 = std::max(absBelow, absRight);

      if (absCoeff == pred1)
      {
        absCoeffMod = 1;
      }
      else
      {
        absCoeffMod = absCoeff < pred1 ? absCoeff + 1 : absCoeff;
      }
    }

    return(absCoeffMod);
  }

  int decDeriveModCoeff(int rightPixel, int belowPixel, int absCoeff)
  {
    int pred1, absBelow = abs(belowPixel), absRight = abs(rightPixel);
    pred1 = std::max(absBelow, absRight);

    int absCoeffMod;

    if (absCoeff == 1 && pred1 > 0)
    {
      absCoeffMod = pred1;
    }
    else
    {
      absCoeffMod = absCoeff - (absCoeff <= pred1);
    }
    return(absCoeffMod);
  }
#endif

  unsigned templateAbsSumTS( int scanPos, const TCoeff* coeff )
  {
    const uint32_t  posY  = m_scan[scanPos].y;
    const uint32_t  posX  = m_scan[scanPos].x;
    const TCoeff*   posC  = coeff + posX + posY * m_width;
    int             sum   = 0;
    if (posX > 0)
    {
      sum += abs(posC[-1]);
    }
    if (posY > 0)
    {
      sum += abs(posC[-(int)m_width]);
    }

    const uint32_t auiGoRicePars[32] =
    {
      0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 2, 2, 2, 2, 2, 2, 2
    };

    return auiGoRicePars[ std::min(sum, 31) ];
  }

#if JVET_O0052_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT
  int                       regBinLimit;
#endif

private:
  // constant
  const ComponentID         m_compID;
  const ChannelType         m_chType;
  const unsigned            m_width;
  const unsigned            m_height;
  const unsigned            m_log2CGWidth;
  const unsigned            m_log2CGHeight;
  const unsigned            m_log2CGSize;
  const unsigned            m_widthInGroups;
  const unsigned            m_heightInGroups;
  const unsigned            m_log2BlockWidth;
  const unsigned            m_log2BlockHeight;
  const unsigned            m_maxNumCoeff;
  const bool                m_signHiding;
  const bool                m_extendedPrecision;
  const int                 m_maxLog2TrDynamicRange;
  CoeffScanType             m_scanType;
  const ScanElement *       m_scan;
  const ScanElement *       m_scanCG;
  const CtxSet              m_CtxSetLastX;
  const CtxSet              m_CtxSetLastY;
  const unsigned            m_maxLastPosX;
  const unsigned            m_maxLastPosY;
  const int                 m_lastOffsetX;
  const int                 m_lastOffsetY;
  const int                 m_lastShiftX;
  const int                 m_lastShiftY;
  const bool                m_TrafoBypass;
  // modified
  int                       m_scanPosLast;
  int                       m_subSetId;
  int                       m_subSetPos;
  int                       m_subSetPosX;
  int                       m_subSetPosY;
  int                       m_minSubPos;
  int                       m_maxSubPos;
  unsigned                  m_sigGroupCtxId;
  int                       m_tmplCpSum1;
  int                       m_tmplCpDiag;
  CtxSet                    m_sigFlagCtxSet[3];
  CtxSet                    m_parFlagCtxSet;
  CtxSet                    m_gtxFlagCtxSet[2];
  unsigned                  m_sigGroupCtxIdTS;
  CtxSet                    m_tsSigFlagCtxSet;
  CtxSet                    m_tsParFlagCtxSet;
  CtxSet                    m_tsGtxFlagCtxSet;
#if JVET_O0122_TS_SIGN_LEVEL
  CtxSet                    m_tsLrg1FlagCtxSet;
  CtxSet                    m_tsSignFlagCtxSet;
#endif
  int                       m_remainingContextBins;
  std::bitset<MLS_GRP_NUM>  m_sigCoeffGroupFlag;
  const bool                m_bdpcm;
};


class CUCtx
{
public:
  CUCtx()              : isDQPCoded(false), isChromaQpAdjCoded(false),
#if JVET_O0472_LFNST_SIGNALLING_LAST_SCAN_POS
                         qgStart(false)
                         {
#if JVET_O0094_LFNST_ZERO_PRIM_COEFFS
                           violatesLfnstConstrained[CHANNEL_TYPE_LUMA  ] = false;
                           violatesLfnstConstrained[CHANNEL_TYPE_CHROMA] = false;
#endif
                           lastScanPos[COMPONENT_Y ] = -1;
                           lastScanPos[COMPONENT_Cb] = -1;
                           lastScanPos[COMPONENT_Cr] = -1;
                         }
#else
                         qgStart(false),
#if JVET_O0094_LFNST_ZERO_PRIM_COEFFS
                         numNonZeroCoeffNonTs(0)
                         {
                           violatesLfnstConstrained[CHANNEL_TYPE_LUMA  ] = false;
                           violatesLfnstConstrained[CHANNEL_TYPE_CHROMA] = false;
                         }
#else
                         numNonZeroCoeffNonTs(0) {}
#endif
#endif
  CUCtx(int _qp)       : isDQPCoded(false), isChromaQpAdjCoded(false),
                         qgStart(false),
#if JVET_O0472_LFNST_SIGNALLING_LAST_SCAN_POS
                         qp(_qp)
                         {
#if JVET_O0094_LFNST_ZERO_PRIM_COEFFS
                           violatesLfnstConstrained[CHANNEL_TYPE_LUMA  ] = false;
                           violatesLfnstConstrained[CHANNEL_TYPE_CHROMA] = false;
#endif
                           lastScanPos[COMPONENT_Y ] = -1;
                           lastScanPos[COMPONENT_Cb] = -1;
                           lastScanPos[COMPONENT_Cr] = -1;
                         }
#else
#if JVET_O0094_LFNST_ZERO_PRIM_COEFFS
                         numNonZeroCoeffNonTs(0), qp(_qp)
                         {
                           violatesLfnstConstrained[CHANNEL_TYPE_LUMA  ] = false;
                           violatesLfnstConstrained[CHANNEL_TYPE_CHROMA] = false;
                         }
#else
                         numNonZeroCoeffNonTs(0), qp(_qp) {}
#endif
#endif
  ~CUCtx() {}
public:
  bool      isDQPCoded;
  bool      isChromaQpAdjCoded;
  bool      qgStart;
#if JVET_O0472_LFNST_SIGNALLING_LAST_SCAN_POS
  int       lastScanPos[MAX_NUM_COMPONENT];
#else
  uint32_t  numNonZeroCoeffNonTs;
#endif
  int8_t    qp;                   // used as a previous(last) QP and for QP prediction
#if JVET_O0094_LFNST_ZERO_PRIM_COEFFS
  bool      violatesLfnstConstrained[MAX_NUM_CHANNEL_TYPE];
#endif
};

class MergeCtx
{
public:
  MergeCtx() : numValidMergeCand( 0 ), hasMergedCandList( false ) { for( unsigned i = 0; i < MRG_MAX_NUM_CANDS; i++ ) mrgTypeNeighbours[i] = MRG_TYPE_DEFAULT_N; }
  ~MergeCtx() {}
public:
  MvField       mvFieldNeighbours [ MRG_MAX_NUM_CANDS << 1 ]; // double length for mv of both lists
  uint8_t       GBiIdx            [ MRG_MAX_NUM_CANDS      ];
  unsigned char interDirNeighbours[ MRG_MAX_NUM_CANDS      ];
  MergeType     mrgTypeNeighbours [ MRG_MAX_NUM_CANDS      ];
  int           numValidMergeCand;
  bool          hasMergedCandList;

  MotionBuf     subPuMvpMiBuf;
  MotionBuf     subPuMvpExtMiBuf;
  MvField mmvdBaseMv[MMVD_BASE_MV_NUM][2];
  void setMmvdMergeCandiInfo(PredictionUnit& pu, int candIdx);
  void setMergeInfo( PredictionUnit& pu, int candIdx );
};

class AffineMergeCtx
{
public:
  AffineMergeCtx() : numValidMergeCand( 0 ) { for ( unsigned i = 0; i < AFFINE_MRG_MAX_NUM_CANDS; i++ ) affineType[i] = AFFINEMODEL_4PARAM; }
  ~AffineMergeCtx() {}
public:
  MvField       mvFieldNeighbours[AFFINE_MRG_MAX_NUM_CANDS << 1][3]; // double length for mv of both lists
  unsigned char interDirNeighbours[AFFINE_MRG_MAX_NUM_CANDS];
  EAffineModel  affineType[AFFINE_MRG_MAX_NUM_CANDS];
  uint8_t       GBiIdx[AFFINE_MRG_MAX_NUM_CANDS];
  int           numValidMergeCand;
  int           maxNumMergeCand;

  MergeCtx     *mrgCtx;
  MergeType     mergeType[AFFINE_MRG_MAX_NUM_CANDS];
};


namespace DeriveCtx
{
void     CtxSplit     ( const CodingStructure& cs, Partitioner& partitioner, unsigned& ctxSpl, unsigned& ctxQt, unsigned& ctxHv, unsigned& ctxHorBt, unsigned& ctxVerBt, bool* canSplit = nullptr );
#if JVET_O0050_LOCAL_DUAL_TREE
unsigned CtxModeConsFlag( const CodingStructure& cs, Partitioner& partitioner );
#endif
#if JVET_O0193_REMOVE_TR_DEPTH_IN_CBF_CTX
unsigned CtxQtCbf     ( const ComponentID compID, const bool prevCbf = false, const int ispIdx = 0 );
#else
unsigned CtxQtCbf     ( const ComponentID compID, const unsigned trDepth, const bool prevCbf = false, const int ispIdx = 0 );
#endif
unsigned CtxInterDir  ( const PredictionUnit& pu );
unsigned CtxSkipFlag  ( const CodingUnit& cu );
unsigned CtxAffineFlag( const CodingUnit& cu );
unsigned CtxPredModeFlag( const CodingUnit& cu );
unsigned CtxIBCFlag(const CodingUnit& cu);
unsigned CtxMipFlag   ( const CodingUnit& cu );
}

#endif // __CONTEXTMODELLING__
