import { createClient, type Session } from "@supabase/supabase-js"
import { useAccount } from "~/account"
import { ArcLabs, type Profile } from "~/account/arclabs"

/**
 * The account's wiring, as the page wakes: Supabase holds the session (signing
 * in, and coming back from Google or an email link), and each session is asked
 * of ArcLabs for its profile. useAccount() reads the state written here.
 */
export default defineNuxtPlugin(async () => {
    const config = useRuntimeConfig().public
    const supabase = createClient(config.supabaseUrl, config.supabaseKey, {
        auth: { flowType: "implicit", autoRefreshToken: true, detectSessionInUrl: true, persistSession: true },
    })
    const session = useState<Session | null | undefined>("session", () => undefined)
    const profile = useState<Profile | null>("profile", () => null)
    const arclabs = ArcLabs({ api: config.api, token: () => session.value?.access_token ?? null })

    // A new person is asked about; a refreshed token for the same one isn't
    async function become(next: Session | null) {
        const same = next?.user.id === session.value?.user.id && profile.value !== null
        session.value = next
        if (!next) profile.value = null
        else if (!same) profile.value = await arclabs.profile()
    }

    const { data, error } = await supabase.auth.getSession()
    if (error) throw error
    await become(data.session)
    // Supabase's callback must not await Supabase: the work is queued behind it
    supabase.auth.onAuthStateChange((_event, next) => void setTimeout(() => void become(next)))

    useAccount().restore()
    return { provide: { supabase: supabase, arclabs: arclabs } }
})
