#include "tex_copy_conv.hpp"

#include "resources.hpp"

#include "../internal.hpp"
#include "../gx/gx.hpp"
#include "../webgpu/gpu.hpp"
#include "../webgpu/gpu_prof.hpp"
#include "texture.hpp"
#include "../gx/gx_fmt.hpp"
#include "efb_wgsl.hpp"

#include <absl/container/flat_hash_map.h>

#include "texture_convert.hpp"

using namespace std::string_literals;

namespace aurora::gfx::tex_copy_conv {
static Module Log("aurora::gfx::tex_copy_conv");

using webgpu::g_device;

static constexpr std::string_view ShaderPreamble = R"(
@group(0) @binding(0) var src_samp: sampler;
@group(0) @binding(1) var src: texture_2d<f32>;

struct Uniforms {
    offset: vec2f,
    scale: vec2f,
    // GX vertical copy filter taps (prev, current, next); w != 0 when active.
    copy_filter: vec4f,
    // x: opaque alpha, y: filter row stride in texels, z/w: clamp UV range for the filter taps.
    flags: vec4f,
};
@group(0) @binding(2) var<uniform> ubuf: Uniforms;

struct VertexOutput {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

var<private> positions: array<vec2f, 3> = array(
    vec2f(-1.0, 1.0),
    vec2f(-1.0, -3.0),
    vec2f(3.0, 1.0),
);
var<private> uvs: array<vec2f, 3> = array(
    vec2f(0.0, 0.0),
    vec2f(0.0, 2.0),
    vec2f(2.0, 0.0),
);

@vertex fn vs_main(@builtin(vertex_index) vi: u32) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4f(positions[vi], 0.0, 1.0);
    out.uv = uvs[vi] * ubuf.scale + ubuf.offset;
    return out;
}

fn apply_opaque_alpha(c: vec4f) -> vec4f {
    return vec4f(c.rgb, select(c.a, 1.0, ubuf.flags.x != 0.0));
}

fn clamp_copy_uv(uv: vec2f) -> vec2f {
    return vec2f(uv.x, clamp(uv.y, ubuf.flags.z, ubuf.flags.w));
}

fn sample_efb(uv: vec2f) -> vec4f {
    let current = textureSample(src, src_samp, clamp_copy_uv(uv));
    if (ubuf.copy_filter.w == 0.0) {
        return apply_opaque_alpha(current);
    }

    let pixel_size = vec2f(1.0, 1.0) / vec2f(textureDimensions(src));
    let row_stride = max(ubuf.flags.y, 1.0);
    let prev = textureSample(src, src_samp, clamp_copy_uv(uv - vec2f(0.0, pixel_size.y * row_stride)));
    let next = textureSample(src, src_samp, clamp_copy_uv(uv + vec2f(0.0, pixel_size.y * row_stride)));
    let prev_rgb = floor(prev.rgb * 255.0 + vec3f(0.5));
    let current_rgb = floor(current.rgb * 255.0 + vec3f(0.5));
    let next_rgb = floor(next.rgb * 255.0 + vec3f(0.5));
    let filtered_rgb = min(floor((prev_rgb * ubuf.copy_filter.x + current_rgb * ubuf.copy_filter.y +
                                  next_rgb * ubuf.copy_filter.z) / 64.0),
                           vec3f(255.0));
    return apply_opaque_alpha(vec4f(filtered_rgb / 255.0, current.a));
}

fn intensity(rgb: vec3f) -> f32 {
    // ITU-R BT.601 luma coefficients
    return dot(rgb, vec3f(0.257, 0.504, 0.098)) + 16.0 / 255.0;
}

fn quantize4(v: f32) -> f32 {
    return floor(v * 16.0) / 15.0;
}
)"sv;

static constexpr std::string_view DepthShaderPreambleBody = R"(
@group(0) @binding(0) var src: texture_depth_2d;

struct UVTransform {
    offset: vec2f,
    scale: vec2f,
    copy_filter: vec4f,
    flags: vec4f,
};
@group(0) @binding(1) var<uniform> uv_xf: UVTransform;

struct VertexOutput {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

var<private> positions: array<vec2f, 3> = array(
    vec2f(-1.0, 1.0),
    vec2f(-1.0, -3.0),
    vec2f(3.0, 1.0),
);
var<private> uvs: array<vec2f, 3> = array(
    vec2f(0.0, 0.0),
    vec2f(0.0, 2.0),
    vec2f(2.0, 0.0),
);

@vertex fn vs_main(@builtin(vertex_index) vi: u32) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4f(positions[vi], 0.0, 1.0);
    out.uv = uvs[vi] * uv_xf.scale + uv_xf.offset;
    return out;
}

fn gx_z24_at_coord(unclamped: vec2i) -> u32 {
    let texSize = vec2i(textureDimensions(src));
    let coord = clamp(unclamped, vec2i(0), texSize - vec2i(1));
    return gx_z24_from_depth(textureLoad(src, coord, 0));
}

fn gx_depth_bytes(z24: u32) -> vec3u {
    return vec3u((z24 >> 16u) & 0xffu, (z24 >> 8u) & 0xffu, z24 & 0xffu);
}

fn clamp_copy_coord(coord: vec2i, texSize: vec2i) -> vec2i {
    let top = i32(floor(uv_xf.flags.z * f32(texSize.y)));
    let bottom = i32(floor(uv_xf.flags.w * f32(texSize.y)));
    return vec2i(coord.x, clamp(coord.y, top, bottom));
}

// Depth copies run through the vertical copy filter too: each Z byte is filtered independently.
fn gx_z24(uv: vec2f) -> u32 {
    let texSize = vec2i(textureDimensions(src));
    let currentCoord = clamp_copy_coord(vec2i(floor(uv * vec2f(texSize))), texSize);
    let current = gx_depth_bytes(gx_z24_at_coord(currentCoord));
    if (uv_xf.copy_filter.w == 0.0) {
        return (current.r << 16u) | (current.g << 8u) | current.b;
    }
    let rowStride = max(i32(round(uv_xf.flags.y)), 1);
    let prev = gx_depth_bytes(gx_z24_at_coord(clamp_copy_coord(currentCoord - vec2i(0, rowStride), texSize)));
    let next = gx_depth_bytes(gx_z24_at_coord(clamp_copy_coord(currentCoord + vec2i(0, rowStride), texSize)));
    let coefficients = vec3u(uv_xf.copy_filter.xyz);
    var filtered = (prev * coefficients.x + current * coefficients.y + next * coefficients.z) >> vec3u(6u);
    // The filter accumulator wraps to nine bits when the taps can reach 512, before saturating.
    if (coefficients.x + coefficients.y + coefficients.z >= 128u) {
        filtered = filtered & vec3u(0x1ffu);
    }
    filtered = min(filtered, vec3u(255u));
    return (filtered.r << 16u) | (filtered.g << 8u) | filtered.b;
}

fn z_byte(z24: u32, shift: u32) -> f32 {
    return f32((z24 >> shift) & 0xFFu) / 255.0;
}
)"sv;

static const std::string DepthShaderPreamble = std::string{efb_wgsl::z24()} + std::string{DepthShaderPreambleBody};

// Passthrough blit (for scaling)
static constexpr std::string_view FragPassthrough = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return sample_efb(in.uv);
}
)"sv;

// GX_TF_I4: 4-bit intensity -> R8Unorm (quantized)
static constexpr std::string_view FragI4 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let rgb = sample_efb(in.uv).rgb;
    let i = quantize4(intensity(rgb));
    return vec4f(i, i, i, i);
}
)"sv;

// GX_TF_I8: 8-bit intensity -> R8Unorm
static constexpr std::string_view FragI8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let rgb = sample_efb(in.uv).rgb;
    let i = intensity(rgb);
    return vec4f(i, i, i, i);
}
)"sv;

// GX_TF_IA4: 4-bit intensity + 4-bit alpha -> RG8Unorm
static constexpr std::string_view FragIA4 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    let i = quantize4(intensity(c.rgb));
    let a = quantize4(c.a);
    return vec4f(i, i, i, a);
}
)"sv;

// GX_TF_IA8: 8-bit intensity + 8-bit alpha -> RG8Unorm
static constexpr std::string_view FragIA8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    let i = intensity(c.rgb);
    return vec4f(i, i, i, c.a);
}
)"sv;

// GX_TF_RGB565: Blit alpha to 1.0
static constexpr std::string_view FragRGB565 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    return vec4f(c.rgb, 1.0);
}
)"sv;

// GX_CTF_R4: 4-bit red -> R8Unorm
static constexpr std::string_view FragR4 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let r = quantize4(sample_efb(in.uv).r);
    return vec4f(r, r, r, r);
}
)"sv;

// GX_CTF_RA4: 4-bit red + 4-bit alpha -> RG8Unorm
static constexpr std::string_view FragRA4 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    let r = quantize4(c.r);
    return vec4f(r, r, r, quantize4(c.a));
}
)"sv;

// GX_CTF_RA8: 8-bit red + 8-bit alpha -> RG8Unorm
static constexpr std::string_view FragRA8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    return vec4f(c.r, c.r, c.r, c.a);
}
)"sv;

// GX_CTF_A8: 8-bit alpha -> R8Unorm
static constexpr std::string_view FragA8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let a = sample_efb(in.uv).a;
    return vec4f(a, a, a, a);
}
)"sv;

// GX_CTF_R8: 8-bit red -> R8Unorm
static constexpr std::string_view FragR8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let r = sample_efb(in.uv).r;
    return vec4f(r, r, r, r);
}
)"sv;

// GX_CTF_G8: 8-bit green -> R8Unorm
static constexpr std::string_view FragG8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let g = sample_efb(in.uv).g;
    return vec4f(g, g, g, g);
}
)"sv;

// GX_CTF_B8: 8-bit blue -> R8Unorm
static constexpr std::string_view FragB8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let b = sample_efb(in.uv).b;
    return vec4f(b, b, b, b);
}
)"sv;

// GX_CTF_RG8: 8-bit red + 8-bit green -> RG8Unorm
static constexpr std::string_view FragRG8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    return vec4f(c.r, c.r, c.r, c.g);
}
)"sv;

// GX_CTF_GB8: 8-bit green + 8-bit blue -> RG8Unorm
static constexpr std::string_view FragGB8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = sample_efb(in.uv);
    return vec4f(c.g, c.g, c.g, c.b);
}
)"sv;

// GX_CTF_Z4: Upper 4 bits of depth -> I4
static constexpr std::string_view FragZ4 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z4 = f32((gx_z24(in.uv) >> 20u) & 0xFu) / 15.0;
    return vec4f(z4, z4, z4, z4);
}
)"sv;

// GX_TF_Z8: Upper 8 bits of depth -> I8
static constexpr std::string_view FragZ8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z8 = z_byte(gx_z24(in.uv), 16u);
    return vec4f(z8, z8, z8, z8);
}
)"sv;

// GX_CTF_Z8M: Middle 8 bits of depth -> I8
static constexpr std::string_view FragZ8M = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z8 = z_byte(gx_z24(in.uv), 8u);
    return vec4f(z8, z8, z8, z8);
}
)"sv;

// GX_CTF_Z8L: Lower 8 bits of depth -> I8
static constexpr std::string_view FragZ8L = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z8 = z_byte(gx_z24(in.uv), 0u);
    return vec4f(z8, z8, z8, z8);
}
)"sv;

// GX_TF_Z16: Upper 16 bits of depth -> IA8 (I = Z[23:16], A = Z[15:8])
static constexpr std::string_view FragZ16 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z24 = gx_z24(in.uv);
    let i = z_byte(z24, 16u);
    return vec4f(i, i, i, z_byte(z24, 8u));
}
)"sv;

// GX_CTF_Z16L: Lower 16 bits of depth -> IA8 (I = Z[15:8], A = Z[7:0])
static constexpr std::string_view FragZ16L = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z24 = gx_z24(in.uv);
    let i = z_byte(z24, 8u);
    return vec4f(i, i, i, z_byte(z24, 0u));
}
)"sv;

// GX_TF_Z24X8: Depth -> RGBA8 (R = Z[23:16], G = Z[15:8], B = Z[7:0])
static constexpr std::string_view FragZ24X8 = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let z24 = gx_z24(in.uv);
    return vec4f(z_byte(z24, 16u), z_byte(z24, 8u), z_byte(z24, 0u), 1.0);
}
)"sv;

// Depth -> R32Float (no scaling)
static constexpr std::string_view DepthSnapshotShader = R"(
@group(0) @binding(0) var src: texture_depth_2d;

var<private> positions: array<vec2f, 3> = array(
    vec2f(-1.0, 1.0),
    vec2f(-1.0, -3.0),
    vec2f(3.0, 1.0),
);

@vertex fn vs_main(@builtin(vertex_index) vi: u32) -> @builtin(position) vec4f {
    return vec4f(positions[vi], 0.0, 1.0);
}

@fragment fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    let depth = textureLoad(src, vec2i(pos.xy), 0);
    return vec4f(depth, 0.0, 0.0, 1.0);
}
)"sv;

static constexpr std::string_view DepthSnapshotShaderMS = R"(
@group(0) @binding(0) var src: texture_depth_multisampled_2d;

var<private> positions: array<vec2f, 3> = array(
    vec2f(-1.0, 1.0),
    vec2f(-1.0, -3.0),
    vec2f(3.0, 1.0),
);

@vertex fn vs_main(@builtin(vertex_index) vi: u32) -> @builtin(position) vec4f {
    return vec4f(positions[vi], 0.0, 1.0);
}

@fragment fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    let depth = textureLoad(src, vec2i(pos.xy), 0);
    return vec4f(depth, 0.0, 0.0, 1.0);
}
)"sv;

struct ConvPipeline {
  GXTexFmt fmt;
  std::string_view fragShader;
  wgpu::TextureFormat outputFormat;
  const char* label;
};

static constexpr std::array ConvPipelines{
    ConvPipeline{GX_TF_I4, FragI4, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv I4"},
    ConvPipeline{GX_TF_I8, FragI8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv I8"},
    ConvPipeline{GX_TF_IA4, FragIA4, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv IA4"},
    ConvPipeline{GX_TF_IA8, FragIA8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv IA8"},
    ConvPipeline{GX_TF_RGB565, FragRGB565, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv RGB565"},
    ConvPipeline{GX_CTF_R4, FragR4, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv R4"},
    ConvPipeline{GX_CTF_RA4, FragRA4, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv RA4"},
    ConvPipeline{GX_CTF_RA8, FragRA8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv RA8"},
    ConvPipeline{GX_CTF_A8, FragA8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv A8"},
    ConvPipeline{GX_CTF_R8, FragR8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv R8"},
    ConvPipeline{GX_CTF_G8, FragG8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv G8"},
    ConvPipeline{GX_CTF_B8, FragB8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv B8"},
    ConvPipeline{GX_CTF_RG8, FragRG8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv RG8"},
    ConvPipeline{GX_CTF_GB8, FragGB8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv GB8"},
};

static constexpr std::array DepthConvPipelines{
    ConvPipeline{GX_CTF_Z4, FragZ4, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z4"},
    ConvPipeline{GX_TF_Z8, FragZ8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z8"},
    ConvPipeline{GX_CTF_Z8M, FragZ8M, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z8M"},
    ConvPipeline{GX_CTF_Z8L, FragZ8L, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z8L"},
    ConvPipeline{GX_TF_Z16, FragZ16, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z16"},
    ConvPipeline{GX_CTF_Z16L, FragZ16L, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z16L"},
    ConvPipeline{GX_TF_Z24X8, FragZ24X8, wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv Z24X8"},
};

static wgpu::BindGroupLayout g_bindGroupLayout;
static wgpu::BindGroupLayout g_depthBindGroupLayout;
static wgpu::BindGroupLayout g_depthBindGroupLayoutMS;
static wgpu::Sampler g_nearestSampler;
static wgpu::Sampler g_linearSampler;
static absl::flat_hash_map<GXTexFmt, wgpu::RenderPipeline> g_pipelines;
static absl::flat_hash_map<GXTexFmt, wgpu::RenderPipeline> g_depthPipelinesMS;
static wgpu::RenderPipeline g_blitPipeline;
static wgpu::BindGroupLayout g_depthSnapshotBindGroupLayout;
static wgpu::BindGroupLayout g_depthSnapshotBindGroupLayoutMS;
static wgpu::RenderPipeline g_depthSnapshotPipeline;
static wgpu::RenderPipeline g_depthSnapshotPipelineMS;

static wgpu::BindGroupLayout create_depth_snapshot_layout(bool multisampled, const char* label) {
  const std::array entries{
      wgpu::BindGroupLayoutEntry{
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture =
              wgpu::TextureBindingLayout{
                  .sampleType = wgpu::TextureSampleType::Depth,
                  .viewDimension = wgpu::TextureViewDimension::e2D,
                  .multisampled = multisampled,
              },
      },
  };
  const wgpu::BindGroupLayoutDescriptor descriptor{
      .label = label,
      .entryCount = entries.size(),
      .entries = entries.data(),
  };
  return g_device.CreateBindGroupLayout(&descriptor);
}

static wgpu::RenderPipeline create_depth_snapshot_pipeline(const std::string_view shaderSource,
                                                           const wgpu::BindGroupLayout& bindGroupLayout,
                                                           const char* label) {
  const std::string source{shaderSource};
  const wgpu::ShaderSourceWGSL wgslSource{wgpu::ShaderSourceWGSL::Init{
      .code = source.c_str(),
  }};
  const wgpu::ShaderModuleDescriptor moduleDescriptor{
      .nextInChain = &wgslSource,
      .label = label,
  };
  const auto module = g_device.CreateShaderModule(&moduleDescriptor);

  const std::array colorTargets{wgpu::ColorTargetState{
      .format = wgpu::TextureFormat::R32Float,
  }};
  const wgpu::FragmentState fragmentState{
      .module = module,
      .entryPoint = "fs_main",
      .targetCount = colorTargets.size(),
      .targets = colorTargets.data(),
  };
  const wgpu::PipelineLayoutDescriptor layoutDescriptor{
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bindGroupLayout,
  };
  const auto pipelineLayout = g_device.CreatePipelineLayout(&layoutDescriptor);
  const wgpu::RenderPipelineDescriptor pipelineDescriptor{
      .label = label,
      .layout = pipelineLayout,
      .vertex =
          wgpu::VertexState{
              .module = module,
              .entryPoint = "vs_main",
          },
      .primitive =
          wgpu::PrimitiveState{
              .topology = wgpu::PrimitiveTopology::TriangleList,
          },
      .fragment = &fragmentState,
  };
  return g_device.CreateRenderPipeline(&pipelineDescriptor);
}

static wgpu::RenderPipeline create_pipeline(const ConvPipeline& conv, const std::string_view shaderPreamble,
                                            const wgpu::BindGroupLayout& bindGroupLayout) {
  std::string shaderSource;
  shaderSource.reserve(shaderPreamble.size() + conv.fragShader.size());
  shaderSource += shaderPreamble;
  shaderSource += conv.fragShader;

  const wgpu::ShaderSourceWGSL wgslSource{wgpu::ShaderSourceWGSL::Init{
      .code = shaderSource.c_str(),
  }};
  const wgpu::ShaderModuleDescriptor moduleDescriptor{
      .nextInChain = &wgslSource,
      .label = conv.label,
  };
  const auto module = g_device.CreateShaderModule(&moduleDescriptor);

  const std::array colorTargets{wgpu::ColorTargetState{
      .format = conv.outputFormat,
  }};
  const wgpu::FragmentState fragmentState{
      .module = module,
      .entryPoint = "fs_main",
      .targetCount = colorTargets.size(),
      .targets = colorTargets.data(),
  };

  const wgpu::PipelineLayoutDescriptor layoutDescriptor{
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bindGroupLayout,
  };
  const auto pipelineLayout = g_device.CreatePipelineLayout(&layoutDescriptor);

  const wgpu::RenderPipelineDescriptor pipelineDescriptor{
      .label = conv.label,
      .layout = pipelineLayout,
      .vertex =
          wgpu::VertexState{
              .module = module,
              .entryPoint = "vs_main",
          },
      .primitive =
          wgpu::PrimitiveState{
              .topology = wgpu::PrimitiveTopology::TriangleList,
          },
      .fragment = &fragmentState,
  };
  return g_device.CreateRenderPipeline(&pipelineDescriptor);
}

bool needs_conversion(const GXTexFmt fmt) { return g_pipelines.contains(fmt); }

void initialize() {
  static constexpr std::array bindGroupLayoutEntries{
      wgpu::BindGroupLayoutEntry{
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler =
              wgpu::SamplerBindingLayout{
                  .type = wgpu::SamplerBindingType::Filtering,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture =
              wgpu::TextureBindingLayout{
                  .sampleType = wgpu::TextureSampleType::Float,
                  .viewDimension = wgpu::TextureViewDimension::e2D,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 2,
          .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
          .buffer =
              wgpu::BufferBindingLayout{
                  .type = wgpu::BufferBindingType::Uniform,
              },
      },
  };
  static constexpr wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
      .label = "TexCopyConv Bind Group Layout",
      .entryCount = bindGroupLayoutEntries.size(),
      .entries = bindGroupLayoutEntries.data(),
  };
  g_bindGroupLayout = g_device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);

  static constexpr std::array depthBindGroupLayoutEntries{
      wgpu::BindGroupLayoutEntry{
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture =
              wgpu::TextureBindingLayout{
                  .sampleType = wgpu::TextureSampleType::Depth,
                  .viewDimension = wgpu::TextureViewDimension::e2D,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 1,
          .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
          .buffer =
              wgpu::BufferBindingLayout{
                  .type = wgpu::BufferBindingType::Uniform,
              },
      },
  };
  static constexpr wgpu::BindGroupLayoutDescriptor depthBindGroupLayoutDescriptor{
      .label = "TexCopyConv Depth Bind Group Layout",
      .entryCount = depthBindGroupLayoutEntries.size(),
      .entries = depthBindGroupLayoutEntries.data(),
  };
  g_depthBindGroupLayout = g_device.CreateBindGroupLayout(&depthBindGroupLayoutDescriptor);
  auto depthEntriesMS = depthBindGroupLayoutEntries;
  depthEntriesMS[0].texture.multisampled = true;
  const wgpu::BindGroupLayoutDescriptor depthDescriptorMS{
      .label = "TexCopyConv Depth MS Bind Group Layout",
      .entryCount = depthEntriesMS.size(),
      .entries = depthEntriesMS.data(),
  };
  g_depthBindGroupLayoutMS = g_device.CreateBindGroupLayout(&depthDescriptorMS);

  g_blitPipeline = create_pipeline(
      {GX_TF_RGBA8, FragPassthrough, webgpu::g_graphicsConfig.surfaceConfiguration.format, "TexCopyConv Blit"},
      ShaderPreamble, g_bindGroupLayout);
  for (const auto& conv : ConvPipelines) {
    g_pipelines[conv.fmt] = create_pipeline(conv, ShaderPreamble, g_bindGroupLayout);
    if (conv.outputFormat != to_wgpu(conv.fmt)) {
      Log.fatal("Output format mismatch for {}", conv.fmt);
    }
  }
  // Skip depth copies in compatibility mode
  if (webgpu::g_hasCoreFeatures) {
    // Match the existing MS depth snapshot path: preserve sample zero rather
    // than averaging depth values across foreground and background surfaces.
    std::string depthPreambleMS = DepthShaderPreamble;
    const auto depthType = depthPreambleMS.find("texture_depth_2d");
    depthPreambleMS.replace(depthType, std::string_view{"texture_depth_2d"}.size(), "texture_depth_multisampled_2d");
    for (const auto& conv : DepthConvPipelines) {
      g_pipelines[conv.fmt] = create_pipeline(conv, DepthShaderPreamble, g_depthBindGroupLayout);
      g_depthPipelinesMS[conv.fmt] = create_pipeline(conv, depthPreambleMS, g_depthBindGroupLayoutMS);
      if (conv.outputFormat != to_wgpu(conv.fmt)) {
        Log.fatal("Output format mismatch for {}", conv.fmt);
      }
    }
    g_depthSnapshotBindGroupLayout = create_depth_snapshot_layout(false, "Depth Snapshot Bind Group Layout");
    g_depthSnapshotBindGroupLayoutMS = create_depth_snapshot_layout(true, "Depth Snapshot MS Bind Group Layout");
    g_depthSnapshotPipeline =
        create_depth_snapshot_pipeline(DepthSnapshotShader, g_depthSnapshotBindGroupLayout, "Depth Snapshot");
    g_depthSnapshotPipelineMS =
        create_depth_snapshot_pipeline(DepthSnapshotShaderMS, g_depthSnapshotBindGroupLayoutMS, "Depth Snapshot MS");
  }

  static constexpr wgpu::SamplerDescriptor nearestSamplerDescriptor{
      .label = "TexCopyConv Nearest Sampler",
      .magFilter = wgpu::FilterMode::Nearest,
      .minFilter = wgpu::FilterMode::Nearest,
  };
  g_nearestSampler = g_device.CreateSampler(&nearestSamplerDescriptor);

  static constexpr wgpu::SamplerDescriptor linearSamplerDescriptor{
      .label = "TexCopyConv Linear Sampler",
      .magFilter = wgpu::FilterMode::Linear,
      .minFilter = wgpu::FilterMode::Linear,
  };
  g_linearSampler = g_device.CreateSampler(&linearSamplerDescriptor);
}

void shutdown() {
  g_pipelines.clear();
  g_depthPipelinesMS.clear();
  g_blitPipeline = {};
  g_bindGroupLayout = {};
  g_depthBindGroupLayout = {};
  g_depthBindGroupLayoutMS = {};
  g_nearestSampler = {};
  g_linearSampler = {};
  g_depthSnapshotBindGroupLayout = {};
  g_depthSnapshotBindGroupLayoutMS = {};
  g_depthSnapshotPipeline = {};
  g_depthSnapshotPipelineMS = {};
}

static void execute(const wgpu::CommandEncoder& cmd, const ConvRequest& req, const wgpu::RenderPipeline& pipeline, bool depthMS = false) {
  if (!pipeline) {
    return;
  }
  wgpu::BindGroup bindGroup;
  if (gx::is_depth_format(req.fmt)) {
    // Skip depth copies in compatibility mode
    if (!webgpu::g_hasCoreFeatures) {
      return;
    }
    const std::array bindGroupEntries{
        wgpu::BindGroupEntry{
            .binding = 0,
            .textureView = req.srcView,
        },
        wgpu::BindGroupEntry{
            .binding = 1,
            .buffer = detail::resources().uniformBuffer,
            .offset = req.uniformRange.offset,
            .size = req.uniformRange.size,
        },
    };
    const wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = depthMS ? g_depthBindGroupLayoutMS : g_depthBindGroupLayout,
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data(),
    };
    bindGroup = g_device.CreateBindGroup(&bindGroupDescriptor);
  } else {
    const auto& sampler = req.sampleFilter == SampleFilter::Linear ? g_linearSampler : g_nearestSampler;
    const std::array bindGroupEntries{
        wgpu::BindGroupEntry{
            .binding = 0,
            .sampler = sampler,
        },
        wgpu::BindGroupEntry{
            .binding = 1,
            .textureView = req.srcView,
        },
        wgpu::BindGroupEntry{
            .binding = 2,
            .buffer = detail::resources().uniformBuffer,
            .offset = req.uniformRange.offset,
            .size = req.uniformRange.size,
        },
    };
    const wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = g_bindGroupLayout,
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data(),
    };
    bindGroup = g_device.CreateBindGroup(&bindGroupDescriptor);
  }

  const std::array colorAttachments{
      wgpu::RenderPassColorAttachment{
          .view = req.dst->attachmentTextureView,
          .loadOp = wgpu::LoadOp::Clear,
          .storeOp = wgpu::StoreOp::Store,
          .clearValue = {0.0, 0.0, 0.0, 0.0},
      },
  };
  const wgpu::RenderPassDescriptor renderPassDescriptor{
      .label = "TexCopyConv Pass",
      .colorAttachmentCount = colorAttachments.size(),
      .colorAttachments = colorAttachments.data(),
      .timestampWrites = webgpu::gpu_prof::pass_writes("EFB copy convert"),
  };
  const auto pass = cmd.BeginRenderPass(&renderPassDescriptor);
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, bindGroup);
  pass.Draw(3);
  pass.End();
}

void run(const wgpu::CommandEncoder& cmd, const ConvRequest& req) {
  const auto it = g_pipelines.find(req.fmt);
  if (it == g_pipelines.end()) {
    Log.fatal("No copy conversion pipeline for format {}", static_cast<int>(req.fmt));
  }
  execute(cmd, req, it->second);
}

void run_depth_multisampled(const wgpu::CommandEncoder& cmd, const ConvRequest& req) {
  const auto it = g_depthPipelinesMS.find(req.fmt);
  if (it == g_depthPipelinesMS.end()) Log.fatal("No MS depth copy conversion pipeline for format {}", static_cast<int>(req.fmt));
  execute(cmd, req, it->second, true);
}

void blit(const wgpu::CommandEncoder& cmd, const ConvRequest& req) { execute(cmd, req, g_blitPipeline); }

bool snapshot_depth_supported() noexcept { return static_cast<bool>(g_depthSnapshotPipeline); }

void snapshot_depth(const wgpu::CommandEncoder& cmd, const wgpu::TextureView& srcDepth, uint32_t msaaSamples,
                    const wgpu::TextureView& dst) {
  const bool multisampled = msaaSamples > 1;
  const auto& pipeline = multisampled ? g_depthSnapshotPipelineMS : g_depthSnapshotPipeline;
  if (!pipeline) {
    return;
  }
  const std::array bindGroupEntries{
      wgpu::BindGroupEntry{
          .binding = 0,
          .textureView = srcDepth,
      },
  };
  const wgpu::BindGroupDescriptor bindGroupDescriptor{
      .layout = multisampled ? g_depthSnapshotBindGroupLayoutMS : g_depthSnapshotBindGroupLayout,
      .entryCount = bindGroupEntries.size(),
      .entries = bindGroupEntries.data(),
  };
  const auto bindGroup = g_device.CreateBindGroup(&bindGroupDescriptor);

  const std::array colorAttachments{
      wgpu::RenderPassColorAttachment{
          .view = dst,
          .loadOp = wgpu::LoadOp::Clear,
          .storeOp = wgpu::StoreOp::Store,
          .clearValue = {0.0, 0.0, 0.0, 0.0},
      },
  };
  const wgpu::RenderPassDescriptor renderPassDescriptor{
      .label = "Depth Snapshot Pass",
      .colorAttachmentCount = colorAttachments.size(),
      .colorAttachments = colorAttachments.data(),
      .timestampWrites = webgpu::gpu_prof::pass_writes("Depth snapshot"),
  };
  const auto pass = cmd.BeginRenderPass(&renderPassDescriptor);
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, bindGroup);
  pass.Draw(3);
  pass.End();
}

} // namespace aurora::gfx::tex_copy_conv
