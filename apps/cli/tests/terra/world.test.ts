import { describe, expect, test } from "bun:test"
import { requests } from "../../src/terra/world"

describe("the world contract's requests", () => {
    test("each request is read into its shape", () => {
        expect(requests.read(`{"type":"state","id":1}`)).toEqual({ type: "state", id: 1 })
        expect(requests.read(`{"type":"place","id":2,"body":"player","name":"terry","token":"t"}`)).toEqual({ type: "place", id: 2, body: "player", name: "terry", token: "t" })
        expect(requests.read(`{"type":"place","id":2,"body":"player","name":"terry"}`)).toEqual({ type: "place", id: 2, body: "player", name: "terry", token: null })
        expect(requests.read(`{"type":"remove","id":3,"name":"terry"}`)).toEqual({ type: "remove", id: 3, name: "terry" })
    })

    test("what isn't one is refused, with its id when it had one", () => {
        expect(requests.read("nope")).toMatchObject({ id: null, code: "WIRE" })
        expect(requests.read(`{"type":"state"}`)).toMatchObject({ id: null, code: "WIRE" })
        expect(requests.read(`{"type":"load","id":4}`)).toMatchObject({ id: 4, code: "CONTRACT_UNSAID" })
        expect(requests.read(`{"type":"place","id":5,"name":"terry"}`)).toMatchObject({ id: 5, code: "WIRE" })
    })
})
