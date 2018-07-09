

#include "guidelineline.h"

// TnzCore includes
#include "tgl.h"


//*****************************************************************************************
//    TGuidelineLineBase implementation
//*****************************************************************************************

TGuidelineLineBase::TGuidelineLineBase(bool enabled, double magnetism, const TPointD &p0, const TPointD &p1):
  TGuideline(enabled, magnetism), p0(p0), p1(p1) { }

TPointD
TGuidelineLineBase::calcDirection(const TPointD &p0, const TPointD &p1) {
  TPointD d = p1 - p0;
  double k = norm2(d);
  return k > TConsts::epsilon*TConsts::epsilon ? d*(1.0/sqrt(k)) : TPointD();
}


static bool
truncateX(double &x0, double &y0, double &x1, double &y1, double minx, double maxx) {
  double dx = x1 - x0;
  if (fabs(dx) < TConsts::epsilon) return false;
  double k = (y1 - y0)/dx;
  if (dx > 0.0) {
    y0 += k*(minx - x0); x0 = minx;
    y1 += k*(maxx - x1); x1 = maxx;
  } else {
    y0 += k*(maxx - x0); x0 = maxx;
    y1 += k*(minx - x1); x1 = minx;
  }
  return true;
}

bool
TGuidelineLineBase::truncateInfiniteLine(const TRectD &bounds, TPointD &p0, TPointD &p1) {
  if (bounds.isEmpty()) return false;
  TPointD d = p0 - p1;
  TDimensionD size = bounds.getSize();
  if (fabs(d.x)*bounds.y0 > bounds.x0*fabs(d.y)) {
    // horizontal
    if (!truncateX(p0.x, p0.y, p1.x, p1.y, bounds.x0, bounds.x1)) return false;
    if (p0.y <= bounds.y0 && p1.y <= bounds.y0) return false;
    if (p0.y >= bounds.y1 && p1.y >= bounds.y1) return false;
  } else {
    // vertical
    if (!truncateX(p0.y, p0.x, p1.y, p1.x, bounds.y0, bounds.y1)) return false;
    if (p0.x <= bounds.x0 && p1.x <= bounds.x0) return false;
    if (p0.x >= bounds.x1 && p1.x >= bounds.x1) return false;
  }
  return true;
}

bool
TGuidelineLineBase::truncateInfiniteRay(const TRectD &bounds, TPointD &p0, TPointD &p1) {
  if (bounds.isEmpty()) return false;
  TRectD b(bounds);
  if (b.contains(p0)) {
    (p0.x < p1.x ? b.x0 : b.x1) = p0.x;
    (p0.y < p1.y ? b.y0 : b.y1) = p0.y;
  }
  return truncateInfiniteLine(b, p0, p1);
}

void
TGuidelineLineBase::drawInfiniteLine(const TPointD &p0, const TPointD &p1, bool ray, bool active, bool enabled) const {
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);

  TAffine matrix = (projection*modelview).get2d();
  TPointD pp0 = matrix*p0;
  TPointD pp1 = matrix*p1;
  if ( ray
     ? !truncateInfiniteRay (TRectD(-1.0, -1.0, 1.0, 1.0), pp0, pp1)
     : !truncateInfiniteLine(TRectD(-1.0, -1.0, 1.0, 1.0), pp0, pp1) ) return;

  double pixelSize = sqrt(tglGetPixelSize2());
  TAffine matrixInv = matrix.inv();
  drawSegment(matrixInv*pp0, matrixInv*pp1, pixelSize, active, enabled);
}


//*****************************************************************************************
//    TGuidelineLine implementation
//*****************************************************************************************

TGuidelineLine::TGuidelineLine(bool enabled, double magnetism, const TPointD &p0, const TPointD &p1):
  TGuidelineLineBase(enabled, magnetism, p0, p1),
  dir(calcDirection(p0, p1)),
  dist(norm(p1 - p0)) { }

TTrackPoint
TGuidelineLine::transformPoint(const TTrackPoint &point) const {
  TTrackPoint p(point);
  p.position = p0 + dir * std::max(0.0, std::min(dist, ((p.position - p0)*dir)));
  return p;
}

void
TGuidelineLine::draw(bool active, bool enabled) const
  { drawSegment(p0, p1, sqrt(tglGetPixelSize2()), active, enabled); }


//*****************************************************************************************
//    TGuidelineInfiniteLine implementation
//*****************************************************************************************

TGuidelineInfiniteLine::TGuidelineInfiniteLine(bool enabled, double magnetism, const TPointD &p0, const TPointD &p1):
  TGuidelineLineBase(enabled, magnetism, p0, p1),
  dir(calcDirection(p0, p1)) { }

TTrackPoint
TGuidelineInfiniteLine::transformPoint(const TTrackPoint &point) const {
  TTrackPoint p(point);
  p.position = p0 + dir * ((p.position - p0)*dir);
  return p;
}

void
TGuidelineInfiniteLine::draw(bool active, bool enabled) const
  { drawInfiniteLine(p0, p1, false, active, enabled); }


//*****************************************************************************************
//    TGuidelineRay implementation
//*****************************************************************************************

TGuidelineRay::TGuidelineRay(bool enabled, double magnetism, const TPointD &p0, const TPointD &p1):
  TGuidelineLineBase(enabled, magnetism, p0, p1),
  dir(calcDirection(p0, p1)) { }

TTrackPoint
TGuidelineRay::transformPoint(const TTrackPoint &point) const {
  TTrackPoint p(point);
  p.position = p0 + dir * std::max(0.0, ((p.position - p0)*dir));
  return p;
}

void
TGuidelineRay::draw(bool active, bool enabled) const
  { drawInfiniteLine(p0, p1, true, active, enabled); }

