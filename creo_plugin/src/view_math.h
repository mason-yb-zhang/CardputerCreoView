#pragma once

namespace ccv::viewmath {

void BuildOrientation(const double anchor[4][4], const float quaternion[4],
                      double output[4][4]);
void BuildPanZoom(const double anchor[4][4], double panX, double panY,
                  double zoomScale, double output[4][4]);

}
