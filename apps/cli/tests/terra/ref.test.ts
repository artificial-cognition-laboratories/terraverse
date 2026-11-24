import { describe, expect, test } from "bun:test"
import { readFileSync } from "node:fs"
import { join } from "node:path"
import { refs, type Ref } from "../../src/terra"

/** The golden both parsers pass: the engine's TestRef reads the same file. */
const golden = JSON.parse(readFileSync(join(import.meta.dir, "../../../game/assets/misc/refs.json"), "utf8")) as {
    refs: { ref: string; is: Ref | null; printed?: string }[]
}

describe("terra:// refs", () => {
    for (const row of golden.refs) {
        test(JSON.stringify(row.ref), () => {
            const parsed = refs.parse(row.ref)
            expect(parsed).toEqual(row.is)
            if (parsed) expect(refs.print(parsed)).toBe(row.printed!)
            if (parsed) expect(refs.parse(refs.print(parsed))).toEqual(parsed)
        })
    }
})
