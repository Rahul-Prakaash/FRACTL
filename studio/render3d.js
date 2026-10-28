(function () {
  "use strict";

  const DEFAULT_PALETTE = { t: ["#6b4a2e"], b: ["#4f9a35"] };

  function hexRgb(h) {
    const n = parseInt(h.slice(1), 16);
    return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
  }
  function srgbToLinear(c) { return c.map((v) => Math.pow(v, 2.2)); }
  function hash01(n) { const x = Math.sin(n * 127.1 + 311.7) * 43758.5453; return x - Math.floor(x); }

  function parseF3D(text) {
    const lines = text.split("\n");
    const tubes = [], blades = [];
    const inst = [];
    let cur = null, order = 0;
    const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
    const grow = (x, y, z) => {
      if (x < lo[0]) lo[0] = x; if (y < lo[1]) lo[1] = y; if (z < lo[2]) lo[2] = z;
      if (x > hi[0]) hi[0] = x; if (y > hi[1]) hi[1] = y; if (z > hi[2]) hi[2] = z;
    };
    for (let li = 0; li < lines.length; li++) {
      const L = lines[li];
      const k = L.charCodeAt(0);
      if (k === 111) {
        const p = L.split(" ");
        const cols = p[3] && p[3] !== "-" ? p[3].split(",") : null;
        cur = { name: p[1], index: inst.length, colors: cols, count: 0, first: order,
                lo: [Infinity, Infinity, Infinity], hi: [-Infinity, -Infinity, -Infinity] };
        inst.push(cur);
        continue;
      }
      if (k !== 116 && k !== 98) continue;
      if (!cur) { cur = { name: "Fractal", index: 0, colors: null, count: 0, first: 0, lo: [Infinity, Infinity, Infinity], hi: [-Infinity, -Infinity, -Infinity] }; inst.push(cur); }
      const v = L.split(" ");
      const a = [+v[1], +v[2], +v[3]], b = [+v[4], +v[5], +v[6]];
      grow(a[0], a[1], a[2]); grow(b[0], b[1], b[2]);
      for (let i = 0; i < 3; i++) {
        cur.lo[i] = Math.min(cur.lo[i], a[i], b[i]);
        cur.hi[i] = Math.max(cur.hi[i], a[i], b[i]);
      }
      const seg = { a, b, inst: cur.index, local: cur.count++, order: order++ };
      if (k === 116) { seg.r = +v[7]; seg.c = +v[8]; tubes.push(seg); }
      else { seg.s = [+v[7], +v[8], +v[9]]; seg.w0 = +v[10]; seg.w1 = +v[11]; seg.c = +v[12]; blades.push(seg); }
    }
    if (!isFinite(lo[0])) { lo.fill(-1); hi.fill(1); }
    let chain = [];
    const flush = () => {
      if (!chain.length) return;
      let total = 0;
      for (const s of chain) total += Math.hypot(s.b[0] - s.a[0], s.b[1] - s.a[1], s.b[2] - s.a[2]);
      let acc = 0;
      for (const s of chain) {
        s.al0 = acc / (total || 1);
        acc += Math.hypot(s.b[0] - s.a[0], s.b[1] - s.a[1], s.b[2] - s.a[2]);
        s.al1 = acc / (total || 1);
        s.len = total;
      }
      chain[chain.length - 1].w1 = 0;
      chain = [];
    };
    for (const s of blades) {
      const p = chain[chain.length - 1];
      if (p && p.inst === s.inst && Math.abs(p.b[0] - s.a[0]) < 2e-3 && Math.abs(p.b[1] - s.a[1]) < 2e-3 &&
          Math.abs(p.b[2] - s.a[2]) < 2e-3) chain.push(s);
      else { flush(); chain = [s]; }
    }
    flush();
    return { tubes, blades, inst, total: order, lo, hi };
  }

  function colorFor(scene, seg, kind) {
    const ins = scene.inst[seg.inst];
    const pal = ins.colors || DEFAULT_PALETTE[kind];
    const base = hexRgb(pal[Math.min(seg.c, pal.length - 1)]);
    const r = hash01(seg.inst * 7.13 + (kind === "b" ? seg.local * 0.37 : 0));
    const k = 0.86 + 0.28 * r;
    return srgbToLinear([base[0] * k, base[1] * (0.94 + 0.12 * r), base[2] * k]);
  }

  function computeBirths(scene, order, duration) {
    const n = scene.total || 1;
    const births = new Float32Array(n);
    let grow;
    if (order === "emission") {
      for (const s of scene.tubes) births[s.order] = (s.order / n) * duration;
      for (const s of scene.blades) births[s.order] = (s.order / n) * duration;
      grow = Math.min(0.6, Math.max(0.06, (duration / n) * 10));
    } else {
      let nmax = 1;
      for (const i of scene.inst) nmax = Math.max(nmax, i.count);
      const start = [], dur = [];
      for (const i of scene.inst) {
        const share = Math.sqrt(i.count / nmax);
        const big = share > 0.6;
        const d = big ? duration * Math.min(1, 0.25 + 0.75 * share) : duration * Math.max(0.08, 0.3 * share);
        dur.push(d);
        start.push(big ? 0 : hash01(i.index * 3.7 + 1.3) * (duration * 0.96 - d));
      }
      const set = (s) => {
        const ins = scene.inst[s.inst];
        births[s.order] = start[s.inst] + (s.local / Math.max(1, ins.count)) * dur[s.inst];
      };
      scene.tubes.forEach(set);
      scene.blades.forEach(set);
      grow = 0.45;
    }
    return { births, grow };
  }

  const M4 = {
    persp(fovy, aspect, near, far) {
      const f = 1 / Math.tan(fovy / 2), nf = 1 / (near - far);
      return new Float32Array([f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) * nf, -1, 0, 0, 2 * far * near * nf, 0]);
    },
    lookAt(e, t, u) {
      let zx = e[0] - t[0], zy = e[1] - t[1], zz = e[2] - t[2];
      let l = Math.hypot(zx, zy, zz); zx /= l; zy /= l; zz /= l;
      let xx = u[1] * zz - u[2] * zy, xy = u[2] * zx - u[0] * zz, xz = u[0] * zy - u[1] * zx;
      l = Math.hypot(xx, xy, xz); xx /= l; xy /= l; xz /= l;
      const yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
      return new Float32Array([xx, yx, zx, 0, xy, yy, zy, 0, xz, yz, zz, 0,
        -(xx * e[0] + xy * e[1] + xz * e[2]), -(yx * e[0] + yy * e[1] + yz * e[2]), -(zx * e[0] + zy * e[1] + zz * e[2]), 1]);
    },
    ortho(l, r, b, t, n, f) {
      return new Float32Array([2 / (r - l), 0, 0, 0, 0, 2 / (t - b), 0, 0, 0, 0, -2 / (f - n), 0,
        -(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1]);
    },
    mul(a, b) {
      const o = new Float32Array(16);
      for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) {
        let s = 0;
        for (let k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
        o[c * 4 + r] = s;
      }
      return o;
    },
    inv(m) {
      const a = m, o = new Float32Array(16);
      const b00 = a[0] * a[5] - a[1] * a[4], b01 = a[0] * a[6] - a[2] * a[4], b02 = a[0] * a[7] - a[3] * a[4];
      const b03 = a[1] * a[6] - a[2] * a[5], b04 = a[1] * a[7] - a[3] * a[5], b05 = a[2] * a[7] - a[3] * a[6];
      const b06 = a[8] * a[13] - a[9] * a[12], b07 = a[8] * a[14] - a[10] * a[12], b08 = a[8] * a[15] - a[11] * a[12];
      const b09 = a[9] * a[14] - a[10] * a[13], b10 = a[9] * a[15] - a[11] * a[13], b11 = a[10] * a[15] - a[11] * a[14];
      let d = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
      if (!d) return o;
      d = 1 / d;
      o[0] = (a[5] * b11 - a[6] * b10 + a[7] * b09) * d; o[1] = (a[2] * b10 - a[1] * b11 - a[3] * b09) * d;
      o[2] = (a[13] * b05 - a[14] * b04 + a[15] * b03) * d; o[3] = (a[10] * b04 - a[9] * b05 - a[11] * b03) * d;
      o[4] = (a[6] * b08 - a[4] * b11 - a[7] * b07) * d; o[5] = (a[0] * b11 - a[2] * b08 + a[3] * b07) * d;
      o[6] = (a[14] * b02 - a[12] * b05 - a[15] * b01) * d; o[7] = (a[8] * b05 - a[10] * b02 + a[11] * b01) * d;
      o[8] = (a[4] * b10 - a[5] * b08 + a[7] * b06) * d; o[9] = (a[1] * b08 - a[0] * b10 - a[3] * b06) * d;
      o[10] = (a[12] * b04 - a[13] * b02 + a[15] * b00) * d; o[11] = (a[9] * b02 - a[8] * b04 - a[11] * b00) * d;
      o[12] = (a[5] * b07 - a[4] * b09 - a[6] * b06) * d; o[13] = (a[0] * b09 - a[1] * b07 + a[2] * b06) * d;
      o[14] = (a[13] * b01 - a[12] * b03 - a[14] * b00) * d; o[15] = (a[8] * b03 - a[9] * b01 + a[10] * b00) * d;
      return o;
    },
  };

  const COMMON = `#version 300 es
precision highp float;
uniform mat4 uVP;
uniform mat4 uLightVP;
uniform vec3 uEye;
uniform float uTime;
uniform float uGrow;
uniform float uClock;
uniform float uWind;
`;
  const LIGHT = `
uniform vec3 uSun;
uniform vec3 uFog;
uniform float uFogFar;
uniform highp sampler2DShadow uShadowMap;
uniform float uShadowOn;
uniform vec2 uShadowTexel;
float shadowAt(vec4 lp) {
  if (uShadowOn < 0.5) return 1.0;
  vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
  if (p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0 || p.z >= 1.0) return 1.0;
  float s = 0.0;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
      s += texture(uShadowMap, vec3(p.xy + vec2(float(x), float(y)) * uShadowTexel * 1.25, p.z - 0.0012));
  return s / 9.0;
}
vec3 lit(vec3 base, vec3 n, vec3 w, float ao, float sh) {
  vec3 v = normalize(uEye - w);
  float sun = max(dot(n, uSun), 0.0);
  float wrap = max(dot(n, uSun) * 0.5 + 0.5, 0.0);
  vec3 hemi = mix(vec3(0.12, 0.10, 0.07), vec3(0.34, 0.41, 0.47), n.y * 0.5 + 0.5);
  vec3 col = base * (hemi * 0.95 * ao + vec3(1.0, 0.86, 0.66) * (sun * 1.75 * sh + wrap * 0.16 * ao));
  col += pow(1.0 - max(dot(n, v), 0.0), 3.0) * (base * 0.8 + vec3(0.01, 0.018, 0.016)) * ao;
  return col;
}
vec3 fogged(vec3 col, vec3 w) {
  return mix(col, uFog, smoothstep(uFogFar * 0.55, uFogFar, length(uEye - w)) * 0.85);
}
vec3 tonemap(vec3 c) { c = c / (c + vec3(0.8)) * 1.35; return pow(c, vec3(1.0 / 2.2)); }
`;

  const TUBE_VS = COMMON + `
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 iA;
layout(location=3) in vec3 iB;
layout(location=4) in float iR;
layout(location=5) in vec3 iC;
layout(location=6) in float iT;
out vec3 vN; out vec3 vW; out vec3 vC; out float vFresh; out vec4 vLight;
out vec3 vBark; out float vRad;
void main() {
  float g = clamp((uTime - iT) / uGrow, 0.0, 1.0);
  if (g <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  vec3 ax = iB - iA; float len = max(length(ax), 1e-5); vec3 H = ax / len;
  vec3 up = abs(H.y) < 0.98 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
  vec3 X = normalize(cross(up, H)); vec3 Z = cross(H, X);
  float r = iR * mix(0.45, 1.0, g) * (1.0 + 0.06 * (1.0 - aPos.y));
  vec3 w = iA + H * (aPos.y * (len * g + r * 0.35)) + (X * aPos.x + Z * aPos.z) * r;
  vN = normalize(X * aNrm.x + Z * aNrm.z);
  vW = w; vC = iC;
  // Bark coordinates in the branch's own frame: around the surface and along the axis,
  // so the texture wraps without a seam and its fissures run along the wood.
  vBark = vec3(aPos.x * iR, dot(iA, vec3(0.31, 1.0, 0.27)) + aPos.y * len * g, aPos.z * iR);
  vRad = iR;
  vFresh = 1.0 - clamp((uTime - iT) / (uGrow * 4.0), 0.0, 1.0);
  vLight = uLightVP * vec4(w, 1.0);
  gl_Position = uVP * vec4(w, 1.0);
}`;

  const BLADE_VS = COMMON + `
layout(location=0) in vec3 aPos;
layout(location=2) in vec3 iA;
layout(location=3) in vec3 iB;
layout(location=4) in vec3 iS;
layout(location=5) in vec2 iW;
layout(location=6) in vec3 iC;
layout(location=7) in float iT;
layout(location=8) in vec3 iL;
out vec3 vN; out vec3 vW; out vec3 vC; out float vFresh; out vec4 vLight;
out float vAlong; out float vRand;
void main() {
  float g = clamp((uTime - iT) / uGrow, 0.0, 1.0);
  if (g <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  vec3 ax = iB - iA; float len = max(length(ax), 1e-5); vec3 H = ax / len;
  vec3 S = normalize(iS - H * dot(iS, H) + vec3(1e-6));
  vec3 N = normalize(cross(H, S));
  float y = aPos.y;
  float w = mix(iW.x, iW.y, y) * mix(0.3, 1.0, g);
  float along = mix(iL.x, iL.y, y * g);
  vec3 p = iA + H * (y * len * g * 1.03) + S * (aPos.x * w) + N * (abs(aPos.x) * w * 0.3);
  float phase = iA.x * 0.23 + iA.z * 0.19;
  float sway = sin(uClock * 1.35 + phase) * 0.65 + sin(uClock * 2.8 + iA.x * 0.71 - iA.z * 0.53) * 0.22;
  p += vec3(0.86, 0.0, 0.5) * sway * uWind * along * along * min(iL.z, 6.0) * 0.1;
  vN = normalize(N + S * aPos.x * 1.3);
  vW = p; vC = iC;
  vAlong = along;
  vRand = fract(sin(dot(floor(iA.xz * 7.0), vec2(12.9898, 78.233))) * 43758.5453);
  vFresh = 1.0 - clamp((uTime - iT) / (uGrow * 4.0), 0.0, 1.0);
  vLight = uLightVP * vec4(p, 1.0);
  gl_Position = uVP * vec4(p, 1.0);
}`;

  const SOLID_FS = `#version 300 es
precision highp float;
uniform vec3 uEye;
uniform vec3 uGlow;
` + LIGHT + `
in vec3 vN; in vec3 vW; in vec3 vC; in float vFresh; in vec4 vLight; in vec3 vBark; in float vRad;
out vec4 frag;
float h3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float n3(vec3 p) {
  vec3 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix(h3(i), h3(i + vec3(1, 0, 0)), f.x), mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y),
             mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x), mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void main() {
  vec3 n = normalize(vN);
  if (!gl_FrontFacing) n = -n;
  float ao = mix(0.45, 1.0, clamp(vW.y * 0.35 + 0.25, 0.0, 1.0));
  // Bark: long vertical fissures (noise stretched along the axis), a ridge/crack
  // profile, fine grain on top, and moss creeping up the base of the trunk.
  vec3 q = vBark * vec3(6.0, 0.9, 6.0);
  float ridges = n3(q) * 0.65 + n3(q * 2.3 + 7.1) * 0.35;
  float crack = smoothstep(0.38, 0.62, ridges);
  float grain = n3(vBark * vec3(24.0, 5.0, 24.0));
  float thick = smoothstep(0.05, 0.25, vRad);
  vec3 base = vC * mix(1.0, mix(0.42, 1.18, crack) * (0.88 + 0.24 * grain), thick);
  float moss = thick * smoothstep(3.2, 0.2, vW.y) * smoothstep(0.45, 0.8, n3(vBark * vec3(3.0, 1.2, 3.0) + 3.7))
             * clamp(n.y * 0.5 + 0.6, 0.0, 1.0);
  base = mix(base, vec3(0.05, 0.085, 0.025), moss * 0.7);
  ao *= mix(1.0, 0.75 + 0.25 * crack, thick);
  vec3 col = fogged(lit(base, n, vW, ao, shadowAt(vLight)), vW);
  col = mix(col, uGlow, vFresh * vFresh * 0.9);
  frag = vec4(tonemap(col), 1.0);
}`;

  const BLADE_FS = `#version 300 es
precision highp float;
uniform vec3 uEye;
uniform vec3 uGlow;
` + LIGHT + `
in vec3 vN; in vec3 vW; in vec3 vC; in float vFresh; in vec4 vLight; in float vAlong; in float vRand;
out vec4 frag;
void main() {
  vec3 n = normalize(vN);
  if (!gl_FrontFacing) n = -n;
  float sh = shadowAt(vLight);
  float t = smoothstep(0.0, 1.0, vAlong);
  vec3 c = mix(vC * vec3(0.36, 0.45, 0.33), vC * 1.2 + vec3(0.05, 0.045, 0.0), t);
  float dry = smoothstep(0.84, 0.97, vRand) * smoothstep(0.4, 1.0, vAlong);
  c = mix(c, vec3(0.40, 0.34, 0.15), dry * 0.75);
  float ao = mix(0.5, 1.0, smoothstep(0.0, 0.75, vAlong));
  vec3 col = lit(c, n, vW, ao, sh);
  vec3 v = normalize(uEye - vW);
  float back = pow(max(dot(-v, uSun), 0.0), 2.0) * 0.55 + max(dot(-n, uSun), 0.0) * 0.4;
  col += c * vec3(1.0, 0.95, 0.55) * back * (0.35 + 0.65 * sh) * 1.15;
  col = fogged(col, vW);
  col = mix(col, uGlow, vFresh * vFresh * 0.9);
  frag = vec4(tonemap(col), 1.0);
}`;

  const DEPTH_FS = `#version 300 es
precision mediump float;
void main() {}`;

  const SKY_VS = `#version 300 es
out vec2 vNdc;
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
  vNdc = p; gl_Position = vec4(p, 0.9999, 1.0);
}`;
  const SKY_FS = `#version 300 es
precision highp float;
uniform mat4 uInvVP; uniform vec3 uEye; uniform vec3 uSun; uniform vec3 uFog;
in vec2 vNdc; out vec4 frag;
void main() {
  vec4 p = uInvVP * vec4(vNdc, 1.0, 1.0);
  vec3 d = normalize(p.xyz / p.w - uEye);
  float h = clamp(d.y, -1.0, 1.0);
  vec3 zen = vec3(0.012, 0.03, 0.045), hor = uFog;
  vec3 col = mix(hor, zen, pow(max(h, 0.0), 0.55));
  col += vec3(1.0, 0.62, 0.32) * pow(max(dot(d, uSun), 0.0), 18.0) * 0.35;
  col = mix(col, uFog * 0.7, smoothstep(0.0, -0.15, h));
  col = col / (col + vec3(0.8)) * 1.35;
  frag = vec4(pow(col, vec3(1.0 / 2.2)), 1.0);
}`;

  const GROUND_VS = `#version 300 es
precision highp float;
uniform mat4 uVP; uniform mat4 uLightVP; uniform float uR;
layout(location=0) in vec2 aXZ;
out vec3 vW; out vec4 vLight;
void main() {
  vW = vec3(aXZ.x * uR, 0.0, aXZ.y * uR);
  vLight = uLightVP * vec4(vW, 1.0);
  gl_Position = uVP * vec4(vW, 1.0);
}`;
  const GROUND_FS = `#version 300 es
precision highp float;
uniform vec3 uEye; uniform vec4 uShadow; uniform float uRing;
` + LIGHT + `
in vec3 vW; in vec4 vLight; out vec4 frag;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
  vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
void main() {
  vec2 q = vW.xz;
  float n = noise(q * 0.07) * 0.55 + noise(q * 0.29) * 0.3 + noise(q * 1.9) * 0.15;
  vec3 soil = mix(vec3(0.034, 0.030, 0.018), vec3(0.060, 0.050, 0.028), noise(q * 0.9));
  vec3 moss = mix(vec3(0.028, 0.058, 0.016), vec3(0.075, 0.115, 0.030), n);
  vec3 base = mix(soil, moss, smoothstep(0.12, 0.5, n));
  float rr = length(q - uShadow.xy);
  base *= 1.0 - 0.3 * smoothstep(uShadow.z, uShadow.z * 0.1, rr);
  vec3 col = lit(base, vec3(0.0, 1.0, 0.0), vW, 1.0, shadowAt(vLight));
  float grid = smoothstep(0.03, 0.0, abs(fract(length(q) / uRing + 0.5) - 0.5)) * 0.06 * smoothstep(uRing * 8.0, 0.0, length(q));
  col += vec3(0.35, 0.95, 0.75) * grid;
  frag = vec4(tonemap(fogged(col, vW)), 1.0);
}`;

  const HEAD_VS = `#version 300 es
uniform mat4 uVP; uniform vec3 uHead; uniform float uSize;
void main() { gl_Position = uVP * vec4(uHead, 1.0); gl_PointSize = uSize; }`;
  const HEAD_FS = `#version 300 es
precision highp float;
uniform vec3 uGlow; out vec4 frag;
void main() {
  vec2 p = gl_PointCoord * 2.0 - 1.0; float r = dot(p, p);
  float a = exp(-r * 5.0) + 0.6 * smoothstep(0.08, 0.0, r);
  frag = vec4(uGlow * a, a);
}`;

  function compile(gl, vs, fs) {
    const mk = (type, src) => {
      const s = gl.createShader(type);
      gl.shaderSource(s, src);
      gl.compileShader(s);
      if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s) + "\n" + src);
      return s;
    };
    const p = gl.createProgram();
    gl.attachShader(p, mk(gl.VERTEX_SHADER, vs));
    gl.attachShader(p, mk(gl.FRAGMENT_SHADER, fs));
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
    const u = {};
    const n = gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS);
    for (let i = 0; i < n; i++) { const info = gl.getActiveUniform(p, i); u[info.name] = gl.getUniformLocation(p, info.name); }
    return { p, u };
  }

  function cylinder(sides) {
    const pos = [], nrm = [], idx = [];
    for (let j = 0; j <= 1; j++)
      for (let i = 0; i <= sides; i++) {
        const a = (i / sides) * Math.PI * 2, x = Math.cos(a), z = Math.sin(a);
        pos.push(x, j, z); nrm.push(x, 0, z);
      }
    const row = sides + 1;
    for (let i = 0; i < sides; i++) idx.push(i, i + row, i + 1, i + 1, i + row, i + row + 1);
    return { pos: new Float32Array(pos), nrm: new Float32Array(nrm), idx: new Uint16Array(idx) };
  }
  function bladeMesh(rows) {
    const pos = [], idx = [];
    for (let j = 0; j <= rows; j++) for (const x of [-0.5, 0.5]) pos.push(x, j / rows, 0);
    for (let j = 0; j < rows; j++) { const a = j * 2; idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); }
    return { pos: new Float32Array(pos), idx: new Uint16Array(idx) };
  }

  class Renderer3D {
    constructor(canvas) {
      this.canvas = canvas;
      const gl = canvas.getContext("webgl2", { antialias: true, alpha: false, powerPreference: "high-performance", preserveDrawingBuffer: true });
      if (!gl) throw new Error("This browser has no WebGL2 - the 3D view needs it.");
      this.gl = gl;
      this.gpu = "";
      const dbg = gl.getExtension("WEBGL_debug_renderer_info");
      if (dbg) this.gpu = gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL);

      this.tubeP = compile(gl, TUBE_VS, SOLID_FS);
      this.bladeP = compile(gl, BLADE_VS, BLADE_FS);
      this.tubeDepthP = compile(gl, TUBE_VS, DEPTH_FS);
      this.bladeDepthP = compile(gl, BLADE_VS, DEPTH_FS);
      this.shadowSize = Math.min(2048, gl.getParameter(gl.MAX_TEXTURE_SIZE));
      this.shadowTex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, this.shadowTex);
      gl.texStorage2D(gl.TEXTURE_2D, 1, gl.DEPTH_COMPONENT24, this.shadowSize, this.shadowSize);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_COMPARE_MODE, gl.COMPARE_REF_TO_TEXTURE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_COMPARE_FUNC, gl.LEQUAL);
      this.shadowFbo = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, this.shadowFbo);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, this.shadowTex, 0);
      gl.drawBuffers([gl.NONE]);
      gl.readBuffer(gl.NONE);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      this.quality = true;
      this.skyP = compile(gl, SKY_VS, SKY_FS);
      this.groundP = compile(gl, GROUND_VS, GROUND_FS);
      this.headP = compile(gl, HEAD_VS, HEAD_FS);

      const cyl = cylinder(14), bl = bladeMesh(4);
      this.cyl = { n: cyl.idx.length };
      this.bl = { n: bl.idx.length };
      this.tubeVao = gl.createVertexArray();
      gl.bindVertexArray(this.tubeVao);
      this._attr(0, cyl.pos, 3); this._attr(1, cyl.nrm, 3);
      this.cylIdx = this._index(cyl.idx);
      this.tubeInst = gl.createBuffer();
      this.tubeBirth = gl.createBuffer();
      this.bladeVao = gl.createVertexArray();
      gl.bindVertexArray(this.bladeVao);
      this._attr(0, bl.pos, 3);
      this.blIdx = this._index(bl.idx);
      this.bladeInst = gl.createBuffer();
      this.bladeBirth = gl.createBuffer();

      const disc = [0, 0];
      for (let i = 0; i <= 128; i++) { const a = (i / 128) * Math.PI * 2; disc.push(Math.cos(a), Math.sin(a)); }
      this.groundVao = gl.createVertexArray();
      gl.bindVertexArray(this.groundVao);
      this._attr(0, new Float32Array(disc), 2);
      this.groundN = disc.length / 2;
      this.emptyVao = gl.createVertexArray();
      gl.bindVertexArray(null);

      this.scene = null;
      this.time = 0;
      this.cam = { yaw: 0.65, pitch: 0.32, dist: 60, target: [0, 8, 0] };
      this.home = null;
      this.autoRotate = true;
      this.sun = (() => { const v = [0.62, 0.52, 0.45], l = Math.hypot(...v); return v.map((x) => x / l); })();
      this.fog = [0.035, 0.06, 0.075];
      this.glow = [1.6, 1.25, 0.6];
      this._input();
    }

    _attr(loc, data, size) {
      const gl = this.gl, b = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, b);
      gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
      gl.enableVertexAttribArray(loc);
      gl.vertexAttribPointer(loc, size, gl.FLOAT, false, 0, 0);
    }
    _index(data) {
      const gl = this.gl, b = gl.createBuffer();
      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, b);
      gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, data, gl.STATIC_DRAW);
      return b;
    }
    _inst(vao, buf, data, layout, stride) {
      const gl = this.gl;
      gl.bindVertexArray(vao);
      gl.bindBuffer(gl.ARRAY_BUFFER, buf);
      gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
      let off = 0;
      for (const [loc, size] of layout) {
        gl.enableVertexAttribArray(loc);
        gl.vertexAttribPointer(loc, size, gl.FLOAT, false, stride * 4, off * 4);
        gl.vertexAttribDivisor(loc, 1);
        off += size;
      }
    }
    _birthAttr(vao, buf, data, loc) {
      const gl = this.gl;
      gl.bindVertexArray(vao);
      gl.bindBuffer(gl.ARRAY_BUFFER, buf);
      gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
      gl.enableVertexAttribArray(loc);
      gl.vertexAttribPointer(loc, 1, gl.FLOAT, false, 0, 0);
      gl.vertexAttribDivisor(loc, 1);
    }

    load(text, order, duration) {
      const t0 = performance.now();
      const sc = parseF3D(text);
      const gl = this.gl;
      const tubes = new Float32Array(sc.tubes.length * 10);
      sc.tubes.forEach((s, i) => {
        const c = colorFor(sc, s, "t"), o = i * 10;
        tubes.set(s.a, o); tubes.set(s.b, o + 3); tubes[o + 6] = s.r; tubes.set(c, o + 7);
      });
      const blades = new Float32Array(sc.blades.length * 17);
      sc.blades.forEach((s, i) => {
        const c = colorFor(sc, s, "b"), o = i * 17;
        blades.set(s.a, o); blades.set(s.b, o + 3); blades.set(s.s, o + 6);
        blades[o + 9] = s.w0; blades[o + 10] = s.w1; blades.set(c, o + 11);
        blades[o + 14] = s.al0; blades[o + 15] = s.al1; blades[o + 16] = s.len;
      });
      this._inst(this.tubeVao, this.tubeInst, tubes, [[2, 3], [3, 3], [4, 1], [5, 3]], 10);
      this._inst(this.bladeVao, this.bladeInst, blades, [[2, 3], [3, 3], [4, 3], [5, 2], [6, 3], [8, 3]], 17);
      gl.bindVertexArray(null);
      this.scene = sc;
      this.retime(order, duration);
      this._frame(sc);
      this.uploadMs = performance.now() - t0;
      return { tubes: sc.tubes.length, blades: sc.blades.length, instances: sc.inst.length, total: sc.total };
    }

    retime(order, duration) {
      const sc = this.scene;
      if (!sc) return;
      const { births, grow } = computeBirths(sc, order, duration);
      this.grow = grow;
      this.duration = duration;
      this.end = duration + grow * 4;
      const tb = new Float32Array(sc.tubes.length), bb = new Float32Array(sc.blades.length);
      sc.tubes.forEach((s, i) => (tb[i] = births[s.order]));
      sc.blades.forEach((s, i) => (bb[i] = births[s.order]));
      this._birthAttr(this.tubeVao, this.tubeBirth, tb, 6);
      this._birthAttr(this.bladeVao, this.bladeBirth, bb, 7);
      this.gl.bindVertexArray(null);
      let lead = 0;
      sc.inst.forEach((i, k) => { if (i.count > sc.inst[lead].count) lead = k; });
      const segs = [];
      const push = (s) => { if (order === "emission" || s.inst === lead) segs.push([births[s.order], s.a, s.b, s.inst, s.order]); };
      sc.tubes.forEach(push); sc.blades.forEach(push);
      segs.sort((x, y) => x[0] - y[0]);
      this.headSegs = segs;
    }

    _frame(sc) {
      let big = sc.inst[0];
      for (const i of sc.inst) if (i.count > big.count) big = i;
      const lo = sc.lo, hi = sc.hi;
      const cx = (lo[0] + hi[0]) / 2, cz = (lo[2] + hi[2]) / 2;
      const span = Math.max(hi[0] - lo[0], hi[2] - lo[2], (hi[1] - lo[1]) * 1.3, 4);
      const h = hi[1] - lo[1];
      this.ground = { r: Math.max(span * 2.2, 40), ring: Math.max(2, Math.round(span / 10)) };
      const bw = Math.max(big.hi[0] - big.lo[0], big.hi[2] - big.lo[2]);
      this.shadow = [(big.lo[0] + big.hi[0]) / 2, (big.lo[2] + big.hi[2]) / 2, Math.max(bw * 0.45, 1.5), 0];
      this.fogFar = span * 5 + 80;
      this.home = { yaw: 0.65, pitch: 0.3, dist: span * 1.25 + h * 0.35 + 6, target: [cx, h * 0.38, cz] };
      Object.assign(this.cam, this.home, { target: this.home.target.slice() });
    }

    resetView() { if (this.home) Object.assign(this.cam, this.home, { target: this.home.target.slice() }); }

    setTime(t) { this.time = t; }

    head() {
      const S = this.headSegs;
      if (!S || !S.length) return null;
      let lo = 0, hi = S.length - 1, k = -1;
      while (lo <= hi) { const m = (lo + hi) >> 1; if (S[m][0] <= this.time) { k = m; lo = m + 1; } else hi = m - 1; }
      if (k < 0) return null;
      const [b, a, e, inst, order] = S[k];
      const g = Math.min(1, (this.time - b) / this.grow);
      return { p: [a[0] + (e[0] - a[0]) * g, a[1] + (e[1] - a[1]) * g, a[2] + (e[2] - a[2]) * g], inst, order, index: k };
    }

    render(dt) {
      const gl = this.gl, c = this.canvas;
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const w = Math.max(1, Math.round(c.clientWidth * dpr)), h = Math.max(1, Math.round(c.clientHeight * dpr));
      if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
      gl.viewport(0, 0, w, h);
      if (!this.scene) {
        gl.clearColor(0.027, 0.043, 0.051, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        return;
      }
      if (this.autoRotate && this.playing && !this.dragging) this.cam.yaw += dt * 0.09;

      const cam = this.cam;
      const eye = [cam.target[0] + cam.dist * Math.cos(cam.pitch) * Math.sin(cam.yaw),
                   cam.target[1] + cam.dist * Math.sin(cam.pitch),
                   cam.target[2] + cam.dist * Math.cos(cam.pitch) * Math.cos(cam.yaw)];
      const far = Math.max(500, cam.dist * 8 + (this.ground ? this.ground.r * 2 : 0));
      const vp = M4.mul(M4.persp(0.7, w / h, Math.max(0.05, cam.dist / 400), far), M4.lookAt(eye, cam.target, [0, 1, 0]));
      const fogFar = this.fogFar || 300;

      const sc0 = this.scene;
      const clock = performance.now() / 1000;
      const wind = this.quality ? 1.0 : 0.0;
      const lc = [(sc0.lo[0] + sc0.hi[0]) / 2, (sc0.lo[1] + sc0.hi[1]) / 2, (sc0.lo[2] + sc0.hi[2]) / 2];
      const lr = Math.max(4, Math.hypot(sc0.hi[0] - sc0.lo[0], sc0.hi[1] - sc0.lo[1], sc0.hi[2] - sc0.lo[2]) * 0.55);
      const leye = [lc[0] + this.sun[0] * lr * 2, lc[1] + this.sun[1] * lr * 2, lc[2] + this.sun[2] * lr * 2];
      const lightVP = M4.mul(M4.ortho(-lr, lr, -lr, lr, 0.1, lr * 4), M4.lookAt(leye, lc, [0, 1, 0]));
      const common = (P, vpm) => {
        gl.uniformMatrix4fv(P.u.uVP, false, vpm);
        if (P.u.uLightVP) gl.uniformMatrix4fv(P.u.uLightVP, false, lightVP);
        if (P.u.uTime) gl.uniform1f(P.u.uTime, this.time);
        if (P.u.uGrow) gl.uniform1f(P.u.uGrow, this.grow);
        if (P.u.uClock) gl.uniform1f(P.u.uClock, clock);
        if (P.u.uWind) gl.uniform1f(P.u.uWind, wind);
      };
      const lightUniforms = (P) => {
        gl.uniform3fv(P.u.uEye, eye);
        gl.uniform3fv(P.u.uSun, this.sun);
        gl.uniform3fv(P.u.uFog, this.fog);
        gl.uniform1f(P.u.uFogFar, fogFar);
        gl.uniform1f(P.u.uShadowOn, this.quality ? 1 : 0);
        gl.uniform2f(P.u.uShadowTexel, 1 / this.shadowSize, 1 / this.shadowSize);
        gl.uniform1i(P.u.uShadowMap, 0);
      };
      const drawInstances = (tubeP, bladeP, vpm, lit) => {
        for (const [P, vao, idx, n, count, cull] of [
          [tubeP, this.tubeVao, this.cylIdx, this.cyl.n, sc0.tubes.length, true],
          [bladeP, this.bladeVao, this.blIdx, this.bl.n, sc0.blades.length, false]]) {
          if (!count) continue;
          gl.useProgram(P.p);
          common(P, vpm);
          if (lit) { lightUniforms(P); gl.uniform3fv(P.u.uGlow, this.glow); }
          if (cull) gl.enable(gl.CULL_FACE); else gl.disable(gl.CULL_FACE);
          gl.bindVertexArray(vao);
          gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, idx);
          gl.drawElementsInstanced(gl.TRIANGLES, n, gl.UNSIGNED_SHORT, 0, count);
        }
        gl.disable(gl.CULL_FACE);
      };

      if (this.quality) {
        gl.bindFramebuffer(gl.FRAMEBUFFER, this.shadowFbo);
        gl.viewport(0, 0, this.shadowSize, this.shadowSize);
        gl.clear(gl.DEPTH_BUFFER_BIT);
        gl.enable(gl.DEPTH_TEST);
        gl.depthFunc(gl.LEQUAL);
        gl.enable(gl.POLYGON_OFFSET_FILL);
        gl.polygonOffset(2.0, 4.0);
        drawInstances(this.tubeDepthP, this.bladeDepthP, lightVP, false);
        gl.disable(gl.POLYGON_OFFSET_FILL);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, w, h);
      }
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, this.shadowTex);

      gl.clearColor(0.03, 0.05, 0.06, 1);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

      gl.disable(gl.DEPTH_TEST);
      gl.useProgram(this.skyP.p);
      gl.uniformMatrix4fv(this.skyP.u.uInvVP, false, M4.inv(vp));
      gl.uniform3fv(this.skyP.u.uEye, eye);
      gl.uniform3fv(this.skyP.u.uSun, this.sun);
      gl.uniform3fv(this.skyP.u.uFog, this.fog);
      gl.bindVertexArray(this.emptyVao);
      gl.drawArrays(gl.TRIANGLES, 0, 3);

      gl.enable(gl.DEPTH_TEST);
      gl.depthFunc(gl.LEQUAL);
      if (this.ground) {
        const P = this.groundP;
        gl.useProgram(P.p);
        common(P, vp);
        lightUniforms(P);
        gl.uniform1f(P.u.uR, this.ground.r);
        gl.uniform4fv(P.u.uShadow, this.shadow);
        gl.uniform1f(P.u.uRing, this.ground.ring);
        gl.bindVertexArray(this.groundVao);
        gl.enable(gl.POLYGON_OFFSET_FILL);
        gl.polygonOffset(1, 1);
        gl.drawArrays(gl.TRIANGLE_FAN, 0, this.groundN);
        gl.disable(gl.POLYGON_OFFSET_FILL);
      }

      const sc = this.scene;
      if (sc) {
        drawInstances(this.tubeP, this.bladeP, vp, true);

        const hd = this.time < this.end ? this.head() : null;
        if (hd) {
          gl.disable(gl.DEPTH_TEST);
          gl.enable(gl.BLEND);
          gl.blendFunc(gl.ONE, gl.ONE);
          gl.useProgram(this.headP.p);
          gl.uniformMatrix4fv(this.headP.u.uVP, false, vp);
          gl.uniform3fv(this.headP.u.uHead, hd.p);
          gl.uniform1f(this.headP.u.uSize, 34 * dpr);
          gl.uniform3fv(this.headP.u.uGlow, [1.0, 0.85, 0.45]);
          gl.bindVertexArray(this.emptyVao);
          gl.drawArrays(gl.POINTS, 0, 1);
          gl.disable(gl.BLEND);
        }
      }
      gl.bindVertexArray(null);
    }

    _input() {
      const c = this.canvas;
      let last = null, mode = 0, pinch = 0;
      c.addEventListener("contextmenu", (e) => e.preventDefault());
      c.addEventListener("pointerdown", (e) => {
        c.setPointerCapture(e.pointerId);
        last = [e.clientX, e.clientY];
        mode = e.button === 2 || e.shiftKey ? 2 : 1;
        this.dragging = true;
      });
      c.addEventListener("pointermove", (e) => {
        if (!last) return;
        const dx = e.clientX - last[0], dy = e.clientY - last[1];
        last = [e.clientX, e.clientY];
        const cam = this.cam;
        if (mode === 1) {
          cam.yaw -= dx * 0.006;
          cam.pitch = Math.min(1.45, Math.max(0.02, cam.pitch + dy * 0.005));
        } else {
          const s = cam.dist * 0.0016;
          const cy = Math.cos(cam.yaw), sy = Math.sin(cam.yaw);
          cam.target[0] -= (dx * cy) * s; cam.target[2] += (dx * sy) * s;
          cam.target[1] += dy * s;
        }
      });
      const up = () => { last = null; this.dragging = false; };
      c.addEventListener("pointerup", up);
      c.addEventListener("pointercancel", up);
      c.addEventListener("wheel", (e) => {
        e.preventDefault();
        this.cam.dist = Math.min(4000, Math.max(1, this.cam.dist * Math.exp(e.deltaY * 0.0012)));
      }, { passive: false });
      c.addEventListener("dblclick", () => this.resetView());
      c.addEventListener("touchmove", (e) => {
        if (e.touches.length !== 2) return;
        const d = Math.hypot(e.touches[0].clientX - e.touches[1].clientX, e.touches[0].clientY - e.touches[1].clientY);
        if (pinch) this.cam.dist = Math.min(4000, Math.max(1, this.cam.dist * (pinch / d)));
        pinch = d;
      }, { passive: true });
      c.addEventListener("touchend", () => (pinch = 0));
    }

    exportOBJ() {
      const sc = this.scene;
      if (!sc) return "";
      const out = ["# FRACTL Studio export - " + sc.total + " segments", "# tubes as 8-sided cylinders, blades as quads"];
      let v = 1;
      const sides = 8;
      for (const s of sc.tubes) {
        const ax = [s.b[0] - s.a[0], s.b[1] - s.a[1], s.b[2] - s.a[2]];
        const l = Math.hypot(...ax) || 1, H = ax.map((x) => x / l);
        const up = Math.abs(H[1]) < 0.98 ? [0, 1, 0] : [1, 0, 0];
        let X = [up[1] * H[2] - up[2] * H[1], up[2] * H[0] - up[0] * H[2], up[0] * H[1] - up[1] * H[0]];
        const xl = Math.hypot(...X); X = X.map((x) => x / xl);
        const Z = [H[1] * X[2] - H[2] * X[1], H[2] * X[0] - H[0] * X[2], H[0] * X[1] - H[1] * X[0]];
        for (const P of [s.a, s.b])
          for (let i = 0; i < sides; i++) {
            const a = (i / sides) * Math.PI * 2, cx = Math.cos(a) * s.r, cz = Math.sin(a) * s.r;
            out.push(`v ${(P[0] + X[0] * cx + Z[0] * cz).toFixed(3)} ${(P[1] + X[1] * cx + Z[1] * cz).toFixed(3)} ${(P[2] + X[2] * cx + Z[2] * cz).toFixed(3)}`);
          }
        for (let i = 0; i < sides; i++) {
          const j = (i + 1) % sides;
          out.push(`f ${v + i} ${v + j} ${v + sides + j} ${v + sides + i}`);
        }
        v += sides * 2;
      }
      for (const s of sc.blades) {
        const h0 = s.w0 / 2, h1 = s.w1 / 2;
        for (const [P, hw] of [[s.a, h0], [s.b, h1]]) {
          out.push(`v ${(P[0] - s.s[0] * hw).toFixed(3)} ${(P[1] - s.s[1] * hw).toFixed(3)} ${(P[2] - s.s[2] * hw).toFixed(3)}`);
          out.push(`v ${(P[0] + s.s[0] * hw).toFixed(3)} ${(P[1] + s.s[1] * hw).toFixed(3)} ${(P[2] + s.s[2] * hw).toFixed(3)}`);
        }
        out.push(`f ${v} ${v + 1} ${v + 3} ${v + 2}`);
        v += 4;
      }
      return out.join("\n") + "\n";
    }
  }

  window.Renderer3D = Renderer3D;
})();
