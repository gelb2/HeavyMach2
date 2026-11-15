#!/usr/bin/env node
// HM2-local offline policy runner. No game runtime or network dependency.
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const file=fileURLToPath(new URL('./presentation.py',import.meta.url));
const run=spawnSync('python3',[file,...process.argv.slice(2)],{stdio:'inherit'});
if(run.error){process.stderr.write(run.error.message+'\n');process.exit(2);}
process.exit(run.status??2);
