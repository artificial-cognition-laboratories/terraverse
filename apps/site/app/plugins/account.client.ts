import { useAccount } from "~/account"

// The account lives in this browser: read it once, as the page wakes
export default defineNuxtPlugin(() => {
    useAccount().restore()
})
