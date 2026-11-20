export default defineMod({
    title: "Hello",
    settings: {
        greeting: { type: "string", default: "Hello", description: "What players are greeted with." },
        loud: { type: "boolean", default: false },
    },
})
