import type { Server } from "bun"
import type { Wearing } from "./body"
import type { BodiesT } from "./bodies"

/**
 * Bridge — where minds wear bodies: one WebSocket server on this machine, a body at
 * `/body/<name>`, a leased one only with its token (`?token=`). One mind a body: a second is
 * refused while the first wears it.
 */
export function Bridge(opts: { bodies: BodiesT }) {
    let server: Server<Wearing> | null = null

    /** Listening, and where: a body is worn at `<where>/body/<name>`. */
    function open(): string {
        server = Bun.serve<Wearing, never>({
            hostname: "127.0.0.1",
            port: 0,
            fetch(request, s) {
                const url = new URL(request.url)
                const m = /^\/body\/([A-Za-z0-9_-]{1,20})$/.exec(url.pathname)
                if (!m) return new Response("a body is at /body/<name>", { status: 404 })
                const name = m[1]!
                const body = opts.bodies.get(name)
                if (!body) return new Response(`no body is placed as ${name}`, { status: 404 })
                if (body.token !== null && url.searchParams.get("token") !== body.token) return new Response(`${name} is leased: its token, please`, { status: 403 })
                if (body.worn) return new Response(`${name} is already worn`, { status: 409 })
                if (s.upgrade(request, { data: { name: name } })) return undefined
                return new Response("a body is worn over a WebSocket", { status: 426 })
            },
            websocket: {
                open(socket) {
                    const body = opts.bodies.get(socket.data.name)
                    if (!body) return socket.close(1011, "the body's gone")
                    void body.wear(socket).catch(cause => socket.close(1011, String(cause instanceof Error ? cause.message : cause).slice(0, 120)))
                },
                message(socket, message) {
                    opts.bodies.get(socket.data.name)?.heard(typeof message === "string" ? message : new TextDecoder().decode(message))
                },
                close(socket) {
                    opts.bodies.get(socket.data.name)?.unwear(socket)
                },
            },
        })
        return `ws://127.0.0.1:${server.port}`
    }

    return {
        open: open,
        close(): void {
            void server?.stop(true)
            server = null
        },
    }
}

export type BridgeT = ReturnType<typeof Bridge>
