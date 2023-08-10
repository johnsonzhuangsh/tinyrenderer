#include "our_gl.h"

SMatrix<4,4> g_m4x4ModelView;
SMatrix<4,4> g_m4x4ViewPort;
SMatrix<4,4> g_m4x4Project;

void CreateViewportMatrix(const int x, const int y, const int w, const int h) {
    g_m4x4ViewPort = {{{w/2., 0, 0, x+w/2.}, {0, h/2., 0, y+h/2.}, {0,0,1,0}, {0,0,0,1}}};
}

void CreateProjectMatrix(const double f) { // check https://en.wikipedia.org/wiki/Camera_matrix
    g_m4x4Project = {{{1,0,0,0}, {0,-1,0,0}, {0,0,1,0}, {0,0,-1/f,0}}};
}

// check https://github.com/ssloy/tinyrenderer/wiki/Lesson-5-Moving-the-camera
void CreateViewTransformMatrix(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 z = (center - eye).normalized();
    vec3 x = cross(up, z).normalized();
    vec3 y = cross(z,  x).normalized();
    SMatrix<4,4> Minv = {{{x.x,x.y,x.z,0}, {y.x,y.y,y.z,0}, {z.x,z.y,z.z,0}, {0,0,0,1}}};
    SMatrix<4,4> Tr   = {{{1,0,0,-eye.x},  {0,1,0,-eye.y},  {0,0,1,-eye.z},  {0,0,0,1}}};
    g_m4x4ModelView = Minv * Tr;
}

vec3 GetBcCoordinates(const vec2 i_vec2Tri[3], const vec2 i_vec2Pos) {
    SMatrix<3,3> ABC = {{embed<3>(i_vec2Tri[0]), embed<3>(i_vec2Tri[1]), embed<3>(i_vec2Tri[2])}};

    // for a degenerate triangle generate negative coordinates, it will be thrown away by the rasterizator
    if (ABC.det() < 1e-3) {
        return {-1, 1, 1};
    }

    return ABC.invert_transpose() * embed<3>(i_vec2Pos);
}

void RasterizeTriangle(
    const vec4           i_vec4ClipVtxs[3],
    SIShader&            i_sPixelShader,
    STgaImage&           i_sRenderTgt,
    std::vector<double>& i_vecZBuffer)
{
    // triangle screen coordinates before/after perspective division
    vec4 vec4TriVtxB4PersDiv[3] = {
        g_m4x4ViewPort * i_vec4ClipVtxs[0],
        g_m4x4ViewPort * i_vec4ClipVtxs[1],
        g_m4x4ViewPort * i_vec4ClipVtxs[2]};
    vec2 vec2TriVtxPostPersDiv[3] = {
        proj<2>(vec4TriVtxB4PersDiv[0] / vec4TriVtxB4PersDiv[0][3]),
        proj<2>(vec4TriVtxB4PersDiv[1] / vec4TriVtxB4PersDiv[1][3]),
        proj<2>(vec4TriVtxB4PersDiv[2] / vec4TriVtxB4PersDiv[2][3])};

    // calculate bounding box
    int iBboxMin[2] = {i_sRenderTgt.width()-1, i_sRenderTgt.height()-1};
    int iBboxMax[2] = {0, 0};
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 2; y++) {
            iBboxMin[y] = std::min(iBboxMin[y], static_cast<int>(vec2TriVtxPostPersDiv[x][y]));
            iBboxMax[y] = std::max(iBboxMax[y], static_cast<int>(vec2TriVtxPostPersDiv[x][y]));
        }
    }

    // rasterize the triangle area
    #pragma omp parallel for
    for (int x = std::max(iBboxMin[0], 0); x <= std::min(iBboxMax[0], i_sRenderTgt.width()-1); x++) {
        for (int y = std::max(iBboxMin[1], 0); y <= std::min(iBboxMax[1], i_sRenderTgt.height()-1); y++) {
            // calculate pixel/fragment pos/depth
            vec3 vec3BcVtx = GetBcCoordinates(vec2TriVtxPostPersDiv, {static_cast<double>(x), static_cast<double>(y)});
            vec3 vec3BcClipVtx = {vec3BcVtx.x/vec4TriVtxB4PersDiv[0][3], vec3BcVtx.y/vec4TriVtxB4PersDiv[1][3], vec3BcVtx.z/vec4TriVtxB4PersDiv[2][3]};
            vec3BcClipVtx = vec3BcClipVtx/(vec3BcClipVtx.x+vec3BcClipVtx.y+vec3BcClipVtx.z); // check https://github.com/ssloy/tinyrenderer/wiki/Technical-difficulties-linear-interpolation-with-perspective-deformations
            double dFragmentDepth = vec3{i_vec4ClipVtxs[0][2], i_vec4ClipVtxs[1][2], i_vec4ClipVtxs[2][2]} * vec3BcClipVtx;

            // filter/discard detection per pos/depth
            if (vec3BcVtx.x < 0                                             ||    // vertex pos invalid
                vec3BcVtx.y < 0                                             ||
                vec3BcVtx.z < 0                                             ||
                dFragmentDepth > i_vecZBuffer[x + y * i_sRenderTgt.width()] ){    // pixel is too far
                continue;
            }

            // color/depth calculation/update
            {
                // calculate color
                STgaColor sPixelColor;
                if (i_sPixelShader.fragment(vec3BcClipVtx, sPixelColor))
                    // fragment shader (PS) can discard current fragment
                    continue;

                // update depth
                i_vecZBuffer[x+y*i_sRenderTgt.width()] = dFragmentDepth;

                // set pixel color to render target
                i_sRenderTgt.set(x, y, sPixelColor);
            }
        }
    }
}

