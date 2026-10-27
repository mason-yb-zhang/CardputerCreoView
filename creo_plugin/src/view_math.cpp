#include "view_math.h"

#include <algorithm>
#include <cmath>

namespace ccv::viewmath {
namespace {

void QuaternionToRowMatrix(const float* input, double matrix[3][3]) {
    double x = input[0];
    double y = input[1];
    double z = input[2];
    double w = input[3];
    double norm = std::sqrt(x * x + y * y + z * z + w * w);
    x /= norm;
    y /= norm;
    z /= norm;
    w /= norm;

    matrix[0][0] = 1.0 - 2.0 * (y * y + z * z);
    matrix[0][1] = 2.0 * (x * y + z * w);
    matrix[0][2] = 2.0 * (x * z - y * w);
    matrix[1][0] = 2.0 * (x * y - z * w);
    matrix[1][1] = 1.0 - 2.0 * (x * x + z * z);
    matrix[1][2] = 2.0 * (y * z + x * w);
    matrix[2][0] = 2.0 * (x * z + y * w);
    matrix[2][1] = 2.0 * (y * z - x * w);
    matrix[2][2] = 1.0 - 2.0 * (x * x + y * y);
}

}

void BuildOrientation(const double anchor[4][4], const float quaternion[4],
                      double output[4][4]) {
    double rotation[3][3] = {};
    QuaternionToRowMatrix(quaternion, rotation);
    for (int row = 0; row < 3; ++row) {
        double length = 0.0;
        for (int column = 0; column < 3; ++column) {
            output[row][column] = 0.0;
            for (int i = 0; i < 3; ++i) {
                output[row][column] += anchor[row][i] * rotation[i][column];
            }
            length += output[row][column] * output[row][column];
        }
        length = std::sqrt(length);
        if (length > 0.0) {
            for (int column = 0; column < 3; ++column) {
                output[row][column] /= length;
            }
        }
    }
    output[3][0] = 0.0;
    output[3][1] = 0.0;
    output[3][2] = 0.0;
    output[3][3] = 1.0;
}

void BuildPanZoom(const double anchor[4][4], double panX, double panY,
                  double zoomScale, double output[4][4]) {
    std::copy(&anchor[0][0], &anchor[0][0] + 16, &output[0][0]);
    output[3][0] += panX;
    output[3][1] += panY;
    output[0][0] *= zoomScale;
    output[1][1] *= zoomScale;
    output[2][2] *= zoomScale;
}

}
