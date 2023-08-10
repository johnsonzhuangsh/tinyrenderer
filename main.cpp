#include <limits>
#include "model.h"
#include "our_gl.h"

constexpr int   g_iRenderTgtW = 800; // output image size
constexpr int   g_iRenderTgtH = 800;
constexpr vec3  g_v3LitDir{1,1,1};   // light source
constexpr vec3  g_v3CamPos{1,1,3};   // camera position
constexpr vec3  g_v3CamDir{0,0,0};   // camera direction
constexpr vec3  g_v3CamUp{0,1,0};    // camera camera vector

extern SMatrix<4,4> g_m4x4ModelView; // "OpenGL" state matrices
extern SMatrix<4,4> g_m4x4Project;

struct Shader : SIShader {
    const Model &cModel;
    vec3 uniform_l;           // light direction in view coordinates
    SMatrix<2,3> varying_uv;  // triangle uv coordinates, written by the vertex sShader, read by the fragment sShader
    SMatrix<3,3> varying_nrm; // normal per vertex to be interpolated by FS
    SMatrix<3,3> view_tri;    // triangle in view coordinates

    Shader(const Model &m) : cModel(m) {
        uniform_l = proj<3>((g_m4x4ModelView*embed<4>(g_v3LitDir, 0.))).normalized(); // transform the light vector to view coordinates
    }

    virtual void vertex(const int iface, const int nthvert, vec4& gl_Position) {
        varying_uv.set_col(nthvert, cModel.uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>((g_m4x4ModelView).invert_transpose()*embed<4>(cModel.normal(iface, nthvert), 0.)));
        gl_Position= g_m4x4ModelView*embed<4>(cModel.vert(iface, nthvert));
        view_tri.set_col(nthvert, proj<3>(gl_Position));
        gl_Position = g_m4x4Project*gl_Position;
    }

    virtual bool fragment(const vec3 bar, STgaColor &gl_FragColor) {
        vec3 bn = (varying_nrm*bar).normalized(); // per-vertex normal interpolation
        vec2 uv = varying_uv*bar; // tex coord interpolation

        // for the math refer to the tangent space normal mapping lecture
        // https://github.com/ssloy/tinyrenderer/wiki/Lesson-6bis-tangent-space-normal-mapping
        SMatrix<3,3> AI = SMatrix<3,3>{ {view_tri.col(1) - view_tri.col(0), view_tri.col(2) - view_tri.col(0), bn} }.invert();
        vec3 i = AI * vec3{varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0};
        vec3 j = AI * vec3{varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0};
        SMatrix<3,3> B = SMatrix<3,3>{ {i.normalized(), j.normalized(), bn} }.transpose();

        vec3 n = (B * cModel.normal(uv)).normalized(); // transform the normal from the texture to the tangent space
        double diff = std::max(0., n*uniform_l); // diffuse light intensity
        vec3 r = (n*(n*uniform_l)*2 - uniform_l).normalized(); // reflected light direction, specular mapping is described here: https://github.com/ssloy/tinyrenderer/wiki/Lesson-6-Shaders-for-the-software-renderer
        double spec = std::pow(std::max(-r.z, 0.), 5+sample2D(cModel.specular(), uv)[0]); // specular intensity, note that the camera lies on the z-axis (in view), therefore simple -r.z

        STgaColor c = sample2D(cModel.diffuse(), uv);
        for (int i : {0,1,2})
            gl_FragColor[i] = std::min<int>(10 + c[i]*(diff + spec), 255); // (a bit of ambient light, diff + spec), clamp the result

        return false; // the pixel is not discarded
    }
};

int main(int argc, char** argv) {
    // CLI options checking
    if (2 > argc) {
        std::cerr << "Usage: " << argv[0] << " ../obj/cModel.obj1 ../obj/cModel.obj2" << std::endl;
        return 1;
    }

    // Prepare transformation matrixs + z/c buffers
    CreateViewTransformMatrix(g_v3CamPos, g_v3CamDir, g_v3CamUp);        // build the view matrix
    CreateViewportMatrix(g_iRenderTgtW/8, g_iRenderTgtH/8, g_iRenderTgtW*3/4, g_iRenderTgtH*3/4); // build the Viewport matrix
    CreateProjectMatrix((g_v3CamPos-g_v3CamDir).norm());                 // build the g_m4x4Project matrix

    std::vector<double> vecZBuffer(g_iRenderTgtW*g_iRenderTgtH, std::numeric_limits<double>::max());
    STgaImage sRenderTgt(g_iRenderTgtW, g_iRenderTgtH, STgaImage::RGB);  // output image / render target

    // Rendering with SW pipeline
    for (int iModelIdx = 1; iModelIdx < argc; iModelIdx++) {             // iterate through all input objects
        Model cModel(argv[iModelIdx]);
        Shader sShader(cModel);
        for (int iFaceIdx = 0; iFaceIdx < cModel.nfaces(); iFaceIdx++) { // for every face/triangle
            vec4 vecClipVtxs[3]; // triangle coordinates (clip coordinates), written by VS, read by PS
            for (int iVtxIdx : {0,1,2})
                sShader.vertex(iFaceIdx, iVtxIdx, vecClipVtxs[iVtxIdx]); // call VS for each triangle vertex
            triangle(vecClipVtxs, sShader, sRenderTgt, vecZBuffer);      // actual rasterization routine call
        }
    }

    // Output render target as TGA file
    sRenderTgt.write_tga_file("render_target.tga");

    return 0;
}

