/**
 * A stream a line at a time, as it arrives, blank lines dropped: an engine process's output, or
 * the requests on terra's own stdin. With `until`, reading stops when it settles: a pipe the
 * other end keeps open must not keep this process alive once nothing needs it.
 */
export async function lines(stream: ReadableStream<Uint8Array>, each: (line: string) => void, until?: Promise<unknown>): Promise<void> {
    const reader = stream.getReader()
    void until?.finally(() => reader.cancel().catch(() => {}))
    const decoder = new TextDecoder()
    let rest = ""
    for (;;) {
        const { done, value } = await reader.read()
        if (done) break
        rest += decoder.decode(value, { stream: true })
        const parts = rest.split("\n")
        rest = parts.pop() ?? ""
        for (const line of parts) if (line.trim()) each(line)
    }
    if (rest.trim()) each(rest)
}
