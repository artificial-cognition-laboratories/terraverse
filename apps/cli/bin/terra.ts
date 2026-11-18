#!/usr/bin/env bun
// terra (and terraverse, the same program): make, run and publish Terraverse servers and mods.
import { Cli } from "../src/cli"

// TERRA_ENGINE: the engine's folder, when terra runs from source (bin/terral) rather than beside it
const engine = process.env.TERRA_ENGINE
process.exitCode = await Cli({ exec: process.execPath, ...(engine ? { engine: engine } : {}) }).dispatch(process.argv.slice(2))
