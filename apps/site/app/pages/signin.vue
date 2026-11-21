<script setup lang="ts">
import { returnTo, useAccount } from "~/account"

definePageMeta({ layout: "landing" })
useHead({ title: "Sign in", meta: [{ name: "robots", content: "noindex" }] })

const route = useRoute()
const account = useAccount()
const next = computed(() => returnTo(route.query.next))

// Signed in, this page is done: on to where they were going, or to choose a name first
watch(
    account.user,
    user => {
        if (!user) return
        void navigateTo(user.username ? next.value : { path: "/register", query: { next: next.value } }, { replace: true })
    },
    { immediate: true },
)
</script>

<template>
    <auth-card title="Welcome back" lede="Sign in to star mods, publish your own and play online.">
        <auth-sign action="Sign in" :google="account.google" :submit="account.signIn" />
        <template #fine>
            New here? <NuxtLink :to="{ path: '/register', query: { next: next } }">Create an account</NuxtLink>
        </template>
    </auth-card>
</template>
