#!/usr/bin/env bun
// terra (and terraverse, the same program): make, run and publish Terraverse servers and mods.
import { Cli } from "../src/cli"

// TERRA_ENGINE: the engine's folder, when terra runs from source (bin/terral) rather than beside it
// TERRA_AS: the name it was typed as (terral), for the commands it suggests next
const engine = process.env.TERRA_ENGINE
const as = process.env.TERRA_AS ?? "terra"
process.exitCode = await Cli({ exec: process.execPath, as: as, ...(engine ? { engine: engine } : {}) }).dispatch(process.argv.slice(2))
