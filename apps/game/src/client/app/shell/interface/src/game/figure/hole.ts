import type { InjectionKey, Ref } from "vue"

/**
 * Where a native view shows through a screen's glass: the screen provides it and cuts it out of
 * everything it draws (InventoryScreen); the view inside sets it to its box (FigureWell). The
 * engine draws under the page, so it's seen only where the page draws nothing at all.
 */
export const HOLE: InjectionKey<Ref<DOMRect | null>> = Symbol("hole")
