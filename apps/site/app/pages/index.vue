<template>
    <div>
        <home-hero />
        <home-features />
        <home-land />
        <shared-footer />
    </div>
</template>

<script setup lang="ts">
definePageMeta({ layout: "landing" })
useHead({ title: "" })
</script>
