// A real Terraverse world, drawn live. Each map is baked from the generator
// (bake/); here it's folded onto a cube-sphere of blocks, as the game lays its
// grids round a planet, and lit by one sun.
const VERTEX = `#version 300 es
in vec2 p;
void main() { gl_Position = vec4(p, 0., 1.); }`

const FRAGMENT = `#version 300 es
precision highp float;
uniform vec2 size;
uniform vec3 place;     // the planet on screen: x, y and radius, in pixels
uniform float spin, tilt, blocks, fade;
uniform sampler2D map;
out vec4 color;

const float PI = 3.14159265;
const vec3 SUN = normalize(vec3(.34, .46, .82));

float hash(vec2 q) { return fract(sin(dot(q, vec2(127.1, 311.7))) * 43758.5453); }

// A direction, snapped to the middle of its block on the cube-sphere.
vec3 snap(vec3 d, out vec2 inBlock) {
    vec3 a = abs(d);
    float m = max(a.x, max(a.y, a.z));
    vec3 c = d / m;
    vec3 g = (c + 1.) * .5 * blocks;
    vec3 f = (floor(g) + .5) / blocks * 2. - 1.;
    if (a.x == m) { c.yz = f.yz; inBlock = fract(g.yz); }
    else if (a.y == m) { c.xz = f.xz; inBlock = fract(g.xz); }
    else { c.xy = f.xy; inBlock = fract(g.xy); }
    return normalize(c);
}

vec3 read(vec3 d) {
    vec2 uv = vec2(atan(d.x, d.z) / (2. * PI) + .5, .5 - asin(clamp(d.y, -1., 1.)) / PI);
    return textureLod(map, uv, 0.).rgb;
}

vec3 stars(vec2 q) {
    vec2 cell = floor(q / 2.);
    float h = hash(cell);
    if (h < .9965) return vec3(0.);
    float tint = hash(cell + 7.);
    return mix(vec3(.75, .85, 1.), vec3(1., .85, .7), tint) * (h - .9965) / .0035;
}

vec3 land(float h, float rain, float lat) {
    // the game's own blocks, from palette.json: sand, grass, dry grass, stone, snow
    vec3 sand = vec3(.996, .851, .353), grass = vec3(.643, .667, .231), forest = vec3(.42, .52, .2),
        dry = vec3(.678, .635, .298), rock = vec3(.608, .604, .514), snow = vec3(.82, .886, .91);
    vec3 c = mix(dry, mix(grass, forest, smoothstep(.55, .85, rain)), smoothstep(.22, .48, rain));
    if (h < .018) c = sand;
    c = mix(c, rock, step(.3, h));
    float cold = lat * lat * 1.1 + h * 1.1;
    return mix(c, snow, step(.86, cold));
}

void main() {
    vec2 q = gl_FragCoord.xy;
    // no stars behind the words on a wide screen
    float clear = size.x / size.y > 1.25 ? smoothstep(.44, .56, q.x / size.x) : smoothstep(.2, .4, 1. - q.y / size.y);
    vec3 sky = vec3(.012, .02, .035) + stars(q) * clear * .8;
    vec2 s = (q - place.xy) / place.z;
    float r2 = dot(s, s);

    // the air, seen past the edge
    float past = sqrt(r2) - 1.;
    float glow = exp(-max(past, 0.) * 13.) * clamp(dot(normalize(vec3(s, .35)), SUN) + .5, 0., 1.);
    if (r2 >= 1.) { color = vec4(mix(sky, vec3(.25, .62, 1.), glow * .85 * fade), 1.); return; }

    vec3 n = vec3(s, sqrt(1. - r2));
    float cs = cos(tilt), sn = sin(tilt);
    vec3 d = vec3(n.x, cs * n.y - sn * n.z, sn * n.y + cs * n.z);
    cs = cos(spin); sn = sin(spin);
    d = vec3(cs * d.x + sn * d.z, d.y, -sn * d.x + cs * d.z);

    vec2 inBlock;
    vec3 b = snap(d, inBlock);
    vec3 t = read(b);
    float h = (t.r - .5) * 2.;
    float lat = abs(b.y);
    vec3 side = normalize(cross(b, vec3(.3, 1., .2)));
    vec3 up = cross(side, b);
    float step1 = 2.2 / blocks;
    float east = (read(normalize(b + side * step1)).r - .5) * 2.;
    float north = (read(normalize(b + up * step1)).r - .5) * 2.;
    float wet = t.b;

    vec3 c, light = n;
    float shine = 0.;
    if (h < 0. || wet > .9) {
        float deep = clamp(1. + h * 2.2, 0., 1.);
        c = mix(vec3(.02, .13, .3), vec3(.1, .68, .68), deep * deep * deep);
        if (lat + (hash(floor(b.xz * blocks)) - .5) * .018 > .965) c = vec3(.612, .906, .871);
        else shine = .16;
    } else {
        c = land(h, t.g, lat);
        if (wet > .3) c = vec3(.1, .6, .65);
        // the slope in world space, turned back to the screen
        vec3 g = (side * (h - east) + up * (h - north)) * 2.6;
        cs = cos(-spin); sn = sin(-spin);
        g = vec3(cs * g.x + sn * g.z, g.y, -sn * g.x + cs * g.z);
        cs = cos(-tilt); sn = sin(-tilt);
        g = vec3(g.x, cs * g.y - sn * g.z, sn * g.y + cs * g.z);
        light = normalize(n + g);
    }
    // the block's edge: what makes it read as built, not painted
    float edge = min(min(inBlock.x, 1. - inBlock.x), min(inBlock.y, 1. - inBlock.y));
    c *= .9 + .1 * smoothstep(0., .12, edge);
    c *= .96 + .08 * hash(floor(b.xy * blocks) + floor(b.z * blocks));

    float sun = max(dot(light, SUN), 0.);
    float day = smoothstep(-.28, .12, dot(n, SUN));
    vec3 lit = c * (vec3(1.08, 1.02, .94) * sun * 1.5 + vec3(.07, .1, .16));
    lit *= mix(.05, 1., day);
    lit += vec3(1., .94, .8) * shine * pow(max(dot(n, normalize(SUN + vec3(0., 0., 1.))), 0.), 260.) * day;
    float rim = pow(1. - n.z, 2.6);
    lit = mix(lit, vec3(.3, .62, 1.), rim * .65 * (day * .9 + .03));
    color = vec4(mix(sky, lit, fade), 1.);
    color.rgb = mix(sky, color.rgb, smoothstep(0., 1.5 / place.z, 1. - sqrt(r2)));
}`

export function Planet(canvas, onReady) {
    const gl = canvas.getContext("webgl2", { antialias: false, alpha: false, powerPreference: "low-power" })
    if (!gl) return null

    const shader = (kind, source) => {
        const s = gl.createShader(kind)
        gl.shaderSource(s, source)
        gl.compileShader(s)
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s))
        return s
    }
    const program = gl.createProgram()
    gl.attachShader(program, shader(gl.VERTEX_SHADER, VERTEX))
    gl.attachShader(program, shader(gl.FRAGMENT_SHADER, FRAGMENT))
    gl.linkProgram(program)
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program))
    gl.useProgram(program)

    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer())
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW)
    gl.enableVertexAttribArray(0)
    gl.bindAttribLocation(program, 0, "p")
    gl.linkProgram(program)
    gl.useProgram(program)
    gl.vertexAttribPointer(gl.getAttribLocation(program, "p"), 2, gl.FLOAT, false, 0, 0)
    gl.enableVertexAttribArray(gl.getAttribLocation(program, "p"))

    const at = name => gl.getUniformLocation(program, name)
    const u = { size: at("size"), place: at("place"), spin: at("spin"), tilt: at("tilt"), blocks: at("blocks"), fade: at("fade") }

    gl.bindTexture(gl.TEXTURE_2D, gl.createTexture())
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)

    const still = matchMedia("(prefers-reduced-motion: reduce)").matches
    const state = { spin: 2.4, tilt: -.3, fade: 0, drag: null, push: 0, ready: false, dirty: true }
    let pixel = 2

    function fit() {
        // Chunky on purpose: the same hard pixels as the game.
        pixel = Math.max(1, Math.round((devicePixelRatio || 1) * (innerWidth < 720 ? 1.5 : 2)))
        const w = Math.ceil(canvas.clientWidth * (devicePixelRatio || 1) / pixel)
        const h = Math.ceil(canvas.clientHeight * (devicePixelRatio || 1) / pixel)
        if (canvas.width !== w || canvas.height !== h) {
            canvas.width = w
            canvas.height = h
            gl.viewport(0, 0, w, h)
        }
        state.dirty = true
    }
    addEventListener("resize", fit)
    fit()

    canvas.addEventListener("pointerdown", e => {
        const [x, y] = placeOf()
        const r = canvas.getBoundingClientRect()
        const scale = canvas.width / r.width
        const dx = (e.clientX - r.left) * scale - x, dy = canvas.height - (e.clientY - r.top) * scale - y
        if (dx * dx + dy * dy > placeOf()[2] ** 2) return
        state.drag = { x: e.clientX, y: e.clientY }
        canvas.setPointerCapture(e.pointerId)
        canvas.classList.add("dragging")
    })
    canvas.addEventListener("pointermove", e => {
        if (!state.drag) return
        state.spin -= (e.clientX - state.drag.x) * .005
        state.tilt = Math.max(-1.2, Math.min(1.2, state.tilt + (e.clientY - state.drag.y) * .005))
        state.push = -(e.clientX - state.drag.x) * .005
        state.drag = { x: e.clientX, y: e.clientY }
        state.dirty = true
    })
    const release = () => { state.drag = null; canvas.classList.remove("dragging") }
    canvas.addEventListener("pointerup", release)
    canvas.addEventListener("pointercancel", release)

    // Beside the words when there is room, rising under them when there is not.
    function placeOf() {
        const w = canvas.width, h = canvas.height
        const wide = w / h > 1.25
        const radius = (wide ? Math.min(h * .56, w * .3) : Math.max(w * .72, h * .5))
        const x = wide ? w * .78 : w * .5
        const y = wide ? h * .46 : h * .26 - radius
        return [x, y, radius]
    }

    let loaded = null
    const api = {
        show(name) {
            loaded = name
            return new Promise((done, fail) => {
                const image = new Image()
                image.onerror = () => fail(new Error(`planet: couldn't load ${name}`))
                image.onload = () => {
                    if (loaded !== name) return done()
                    gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE)
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE, image)
                    state.fade = 0
                    state.dirty = true
                    if (!state.ready) { state.ready = true; onReady?.() }
                    done()
                }
                image.src = `worlds/${name}.webp`
            })
        },
    }

    let last = performance.now()
    function frame(now) {
        const dt = Math.min(.05, (now - last) / 1000)
        last = now
        requestAnimationFrame(frame)
        if (!state.ready || document.hidden) return
        if (!still && !state.drag) {
            state.spin += dt * .045 + state.push
            state.push *= .93
            state.dirty = true
        }
        if (state.fade < 1) { state.fade = Math.min(1, state.fade + dt * (still ? 100 : 2.2)); state.dirty = true }
        if (!state.dirty) return
        state.dirty = false
        const [x, y, r] = placeOf()
        gl.uniform2f(u.size, canvas.width, canvas.height)
        gl.uniform3f(u.place, x, y, r)
        gl.uniform1f(u.spin, state.spin)
        gl.uniform1f(u.tilt, state.tilt)
        gl.uniform1f(u.blocks, 256)
        gl.uniform1f(u.fade, state.fade)
        gl.drawArrays(gl.TRIANGLES, 0, 3)
    }
    requestAnimationFrame(frame)
    return api
}
