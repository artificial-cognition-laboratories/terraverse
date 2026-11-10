export interface PlanetView {
    show(name: string): Promise<void>
}

/** A baked world on a cube-sphere of blocks; null when there's no WebGL2. */
export function Planet(canvas: HTMLCanvasElement, onReady?: () => void): PlanetView | null
