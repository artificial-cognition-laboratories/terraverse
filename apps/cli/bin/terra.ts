#!/usr/bin/env bun
// terra (and terraverse, the same program): make, run and publish Terraverse servers and mods.
import { Cli } from "../src/cli"

process.exitCode = await Cli({ exec: process.execPath }).dispatch(process.argv.slice(2))
