(function () {
  "use strict";

  const STOPS = [[142, 240, 196], [104, 214, 214], [168, 200, 140], [246, 178, 107]];
  function ramp(f) {
    const x = Math.min(0.9999, Math.max(0, f)) * (STOPS.length - 1), i = Math.floor(x), t = x - i;
    const a = STOPS[i], b = STOPS[i + 1];
    return `rgb(${(a[0] + (b[0] - a[0]) * t) | 0},${(a[1] + (b[1] - a[1]) * t) | 0},${(a[2] + (b[2] - a[2]) * t) | 0})`;
  }

  class Renderer2D {
    constructor(canvas) {
      this.canvas = canvas;
      this.ctx = canvas.getContext("2d");
      this.cache = document.createElement("canvas");
      this.cctx = this.cache.getContext("2d");
      this.segs = null;
      this.time = 0;
      this.view = { zoom: 1, px: 0, py: 0 };
      this.key = "";
      this._input();
    }

    load(svgText, order, duration) {
      const segs = [];
      let strokes = 0;
      const re = /points="([^"]*)"/g;
      let m;
      while ((m = re.exec(svgText))) {
        const pts = m[1].trim().split(/\s+/).map((p) => p.split(",").map(Number));
        for (let i = 1; i < pts.length; i++) segs.push(pts[i - 1][0], pts[i - 1][1], pts[i][0], pts[i][1]);
        strokes++;
      }
      this.segs = new Float32Array(segs);
      this.n = segs.length / 4;
      let lo = [Infinity, Infinity], hi = [-Infinity, -Infinity];
      for (let i = 0; i < segs.length; i += 2) {
        lo[0] = Math.min(lo[0], segs[i]); hi[0] = Math.max(hi[0], segs[i]);
        lo[1] = Math.min(lo[1], segs[i + 1]); hi[1] = Math.max(hi[1], segs[i + 1]);
      }
      if (!isFinite(lo[0])) { lo = [0, 0]; hi = [1, 1]; }
      this.bbox = { lo, hi };
      this.view = { zoom: 1, px: 0, py: 0 };
      this.retime(order, duration);
      return { segments: this.n, strokes };
    }

    retime(order, duration) {
      this.duration = duration;
      this.grow = Math.min(0.5, Math.max(0.04, (duration / Math.max(1, this.n)) * 12));
      this.end = duration + this.grow;
      this.key = "";
    }

    resetView() { this.view = { zoom: 1, px: 0, py: 0 }; this.key = ""; }
    setTime(t) { this.time = t; }
    birth(i) { return (i / Math.max(1, this.n)) * this.duration; }

    head() {
      if (!this.n) return null;
      const k = Math.min(this.n - 1, Math.floor((this.time / this.duration) * this.n));
      return k < 0 ? null : { index: k };
    }

    _transform(w, h, dpr) {
      const { lo, hi } = this.bbox;
      const padT = 78 * dpr, padB = 92 * dpr, padX = 40 * dpr;
      const bw = Math.max(1e-6, hi[0] - lo[0]), bh = Math.max(1e-6, hi[1] - lo[1]);
      const s = Math.min((w - padX * 2) / bw, (h - padT - padB) / bh) * this.view.zoom;
      const ox = (w - bw * s) / 2 - lo[0] * s + this.view.px * dpr;
      const oy = padT + ((h - padT - padB) - bh * s) / 2 - lo[1] * s + this.view.py * dpr;
      return { s, ox, oy };
    }

    render() {
      const c = this.canvas, ctx = this.ctx;
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const w = Math.max(1, Math.round(c.clientWidth * dpr)), h = Math.max(1, Math.round(c.clientHeight * dpr));
      if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
      ctx.clearRect(0, 0, w, h);
      if (!this.segs) return;
      const T = this._transform(w, h, dpr);
      const lw = Math.max(1, Math.min(2.2, 0.9 + T.s * 0.08)) * dpr;
      const key = `${w}x${h}|${T.s}|${T.ox}|${T.oy}`;
      if (key !== this.key || this.time < this.lastTime) {
        this.key = key;
        this.cache.width = w; this.cache.height = h;
        this.done = 0;
        const g = this.cctx;
        g.clearRect(0, 0, w, h);
        g.fillStyle = "rgba(142,240,196,0.07)";
        const step = 26 * dpr;
        for (let x = (T.ox % step); x < w; x += step) for (let y = (T.oy % step); y < h; y += step) g.fillRect(x, y, dpr, dpr);
      }
      this.lastTime = this.time;

      const g = this.cctx, S = this.segs, n = this.n;
      g.lineCap = "round"; g.lineJoin = "round"; g.lineWidth = lw;
      while (this.done < n && this.birth(this.done) + this.grow <= this.time) {
        const i = this.done * 4;
        g.strokeStyle = ramp(this.done / n);
        g.beginPath();
        g.moveTo(T.ox + S[i] * T.s, T.oy + S[i + 1] * T.s);
        g.lineTo(T.ox + S[i + 2] * T.s, T.oy + S[i + 3] * T.s);
        g.stroke();
        this.done++;
      }
      ctx.drawImage(this.cache, 0, 0);

      ctx.lineCap = "round";
      let hx = null, hy = null;
      for (let k = this.done; k < n; k++) {
        const b = this.birth(k);
        if (b > this.time) break;
        const f = Math.min(1, (this.time - b) / this.grow), i = k * 4;
        const x0 = T.ox + S[i] * T.s, y0 = T.oy + S[i + 1] * T.s;
        const x1 = x0 + (S[i + 2] - S[i]) * T.s * f, y1 = y0 + (S[i + 3] - S[i + 1]) * T.s * f;
        ctx.strokeStyle = "rgba(255,226,160,0.95)";
        ctx.lineWidth = lw * 1.6;
        ctx.shadowColor = "rgba(255,200,110,0.9)";
        ctx.shadowBlur = 12 * dpr;
        ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
        hx = x1; hy = y1;
      }
      ctx.shadowBlur = 0;
      if (hx !== null && this.time < this.end) {
        const r = 14 * dpr, grd = ctx.createRadialGradient(hx, hy, 0, hx, hy, r);
        grd.addColorStop(0, "rgba(255,240,200,1)");
        grd.addColorStop(0.35, "rgba(255,190,100,0.55)");
        grd.addColorStop(1, "rgba(255,190,100,0)");
        ctx.fillStyle = grd;
        ctx.beginPath(); ctx.arc(hx, hy, r, 0, Math.PI * 2); ctx.fill();
      }
    }

    _input() {
      const c = this.canvas;
      let last = null;
      c.addEventListener("pointerdown", (e) => { c.setPointerCapture(e.pointerId); last = [e.clientX, e.clientY]; });
      c.addEventListener("pointermove", (e) => {
        if (!last) return;
        this.view.px += e.clientX - last[0];
        this.view.py += e.clientY - last[1];
        last = [e.clientX, e.clientY];
      });
      const up = () => (last = null);
      c.addEventListener("pointerup", up);
      c.addEventListener("pointercancel", up);
      c.addEventListener("wheel", (e) => {
        e.preventDefault();
        const r = c.getBoundingClientRect();
        const mx = e.clientX - r.left - r.width / 2 - this.view.px, my = e.clientY - r.top - r.height / 2 - this.view.py;
        const k = Math.exp(-e.deltaY * 0.0015);
        const z = Math.min(40, Math.max(0.2, this.view.zoom * k));
        const kk = z / this.view.zoom;
        this.view.px -= mx * (kk - 1);
        this.view.py -= my * (kk - 1);
        this.view.zoom = z;
      }, { passive: false });
      c.addEventListener("dblclick", () => this.resetView());
    }
  }

  window.Renderer2D = Renderer2D;
})();
