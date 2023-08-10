#include "tgaimage.h"
#include "geometry.h"

void CreateViewportMatrix(const int x, const int y, const int w, const int h);
void CreateProjectMatrix(const double coeff=0); // coeff = -1/c
void CreateViewTransformMatrix(const vec3 eye, const vec3 center, const vec3 up);

struct SIShader {
    static STgaColor sample2D(const STgaImage &img, vec2 &uvf) {
        return img.get(uvf[0] * img.width(), uvf[1] * img.height());
    }
    virtual bool fragment(const vec3 bar, STgaColor &color) = 0;
};

void RasterizeTriangle(const vec4 clip_verts[3], SIShader &shader, STgaImage &image, std::vector<double> &zbuffer);

