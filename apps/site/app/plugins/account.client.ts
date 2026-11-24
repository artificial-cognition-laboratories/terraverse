import { createClient, type Session } from "@supabase/supabase-js"
import { useAccount } from "~/account"
import { ArcLabs, type Profile } from "~/account/arclabs"

/**
 * The account's wiring, as the page wakes: Supabase holds the session (signing
 * in, and coming back from Google or an email link), and each session is asked
 * of ArcLabs for its profile. useAccount() reads the state written here.
 */
export default defineNuxtPlugin(nuxt => {
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

    // Nothing here holds the page up: it's drawn with who was here last, and corrected when the session is read
    const account = useAccount()
    nuxt.hook("app:mounted", () => {
        account.restore()
        void supabase.auth.getSession().then(({ data, error }) => {
            if (error) throw error
            return become(data.session)
        })
        // Supabase's callback must not await Supabase: the work is queued behind it
        supabase.auth.onAuthStateChange((_event, next) => void setTimeout(() => void become(next)))
    })
    // Once the session is read, whoever it is (nobody too) is who's remembered
    watch([account.ready, account.user], ([ready, now]) => {
        if (ready) account.remember(now)
    })

    return { provide: { supabase: supabase, arclabs: arclabs } }
})
