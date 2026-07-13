import { execSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { tmpdir } from 'node:os';

const REPO = 'ThanhTrucSolutions/OriLang';

function sh(cmd) {
  return execSync(cmd, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}

function ensureLabel(name, color, description) {
  try {
    sh(`gh label create ${JSON.stringify(name)} --repo ${REPO} --color ${color} --description ${JSON.stringify(description)}`);
  } catch {
    try {
      sh(`gh label edit ${JSON.stringify(name)} --repo ${REPO} --color ${color} --description ${JSON.stringify(description)}`);
    } catch { /* ignore */ }
  }
}

function createIssue(title, body, labels) {
  const dir = mkdtempSync(join(tmpdir(), 'ori-issue-'));
  const file = join(dir, 'body.md');
  try {
    writeFileSync(file, body, 'utf8');
    const labelFlags = labels.map((l) => `--label ${JSON.stringify(l)}`).join(' ');
    console.log(
      sh(
        `gh issue create --repo ${REPO} --title ${JSON.stringify(title)} --body-file ${JSON.stringify(file)} ${labelFlags}`,
      ),
    );
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

for (const row of [
  ['bounty', '5319E7', 'Eligible for MergeOS MRG bounty'],
  ['bounty: feature', 'A2EEEF', 'Feature bounty'],
  ['bounty: bug', 'D93F0B', 'Bug bounty'],
  ['vm', '1D76DB', 'C orivm'],
  ['compiler', '0E8A16', 'oric compiler'],
  ['cli', 'FBCA04', 'ori CLI'],
  ['docs', '0075CA', 'Documentation'],
  ['samples', 'C5DEF5', 'Sample apps'],
  ['platform', 'BFD4F2', 'Platform target'],
  ['web', '5319E7', 'WASM / web'],
  ['android', 'A2EEEF', 'Android'],
  ['reward:25-mrg', 'FEF2C0', '25 MRG'],
  ['reward:50-mrg', 'FEF2C0', '50 MRG'],
  ['reward:100-mrg', 'FEF2C0', '100 MRG'],
  ['reward:200-mrg', 'FEF2C0', '200 MRG'],
  ['good first issue', '7057FF', 'Good first issue'],
]) ensureLabel(...row);

const footer = `

## Claim

1. Follow https://github.com/mergeos-bounties  
2. Star https://github.com/mergeos-bounties/mergeos  
3. Star https://github.com/mergeos-bounties/mergeos-contracts
4. Comment: \`I claim this bounty\`  
5. MergeOS [Claim #1](https://github.com/mergeos-bounties/mergeos/issues/1) with this issue link  
6. PR to **OriLang** \`main\` with \`Fixes #<n>\`

Policy: [docs/BOUNTY.md](../blob/main/docs/BOUNTY.md)
`;

const issues = [
  {
    title: '[25 MRG] Docs: CONTRIBUTING + Windows build prerequisites',
    labels: ['bounty', 'bounty: feature', 'docs', 'reward:25-mrg', 'good first issue'],
    body: `## 25 MRG\n\nAdd CONTRIBUTING.md: MSVC/build.cmd, Linux/macOS build.sh, first \`ori create\` / \`ori run\`.\n\n## Acceptance\n- [ ] File + README link\n${footer}`,
  },
  {
    title: '[25 MRG] Docs: language cheatsheet (fold/hold/when/loop)',
    labels: ['bounty', 'bounty: feature', 'docs', 'reward:25-mrg', 'good first issue'],
    body: `## 25 MRG\n\nShort docs/CHEATSHEET.md for newcomers.\n\n## Acceptance\n- [ ] Examples compile with oric\n${footer}`,
  },
  {
    title: '[50 MRG] Sample: hello CLI with tests in docs/examples',
    labels: ['bounty', 'bounty: feature', 'samples', 'reward:50-mrg'],
    body: `## 50 MRG\n\nMinimal sample under samples/ that \`ori run\` on Windows + notes for Linux.\n\n## Acceptance\n- [ ] README run steps + output screenshot or log\n${footer}`,
  },
  {
    title: '[50 MRG] VM: clearer error messages for stack underflow',
    labels: ['bounty', 'bounty: feature', 'vm', 'reward:50-mrg'],
    body: `## 50 MRG\n\nImprove core/orivm.c diagnostics when bytecode is invalid.\n\n## Acceptance\n- [ ] Minimal repro .ori + message improvement\n${footer}`,
  },
  {
    title: '[100 MRG] Compiler: better parse errors with line/col',
    labels: ['bounty', 'bounty: feature', 'compiler', 'reward:100-mrg'],
    body: `## 100 MRG\n\nSurface line/column in oric errors for common syntax mistakes.\n\n## Acceptance\n- [ ] Before/after on 3 bad snippets\n${footer}`,
  },
  {
    title: '[50 MRG] CLI: ori doctor more checks (paths, orb present)',
    labels: ['bounty', 'bounty: feature', 'cli', 'reward:50-mrg'],
    body: `## 50 MRG\n\nExtend \`ori doctor\` to report missing tools/orivm/orb clearly.\n\n## Acceptance\n- [ ] Screenshot/log of doctor output\n${footer}`,
  },
  {
    title: '[50 MRG] Web sample polish: mobile layout for WASM todo',
    labels: ['bounty', 'bounty: feature', 'web', 'samples', 'reward:50-mrg'],
    body: `## 50 MRG\n\nImprove samples web/todo-web UI for small screens.\n\n## Acceptance\n- [ ] Screenshots desktop + mobile width\n${footer}`,
  },
  {
    title: '[100 MRG] Android sample: build notes + permission UX',
    labels: ['bounty', 'bounty: feature', 'android', 'platform', 'reward:100-mrg'],
    body: `## 100 MRG\n\nDocument and harden samples/mobile-android build path.\n\n## Acceptance\n- [ ] Build log or APK steps; no secrets\n${footer}`,
  },
  {
    title: '[50 MRG] Platform: Linux sample README smoke script',
    labels: ['bounty', 'bounty: feature', 'platform', 'samples', 'reward:50-mrg'],
    body: `## 50 MRG\n\nscripts/smoke-linux.sh or docs for samples/linux weather app.\n\n## Acceptance\n- [ ] Documented run\n${footer}`,
  },
  {
    title: '[100 MRG] Built-in: safer http_get timeouts/errors',
    labels: ['bounty', 'bounty: feature', 'vm', 'reward:100-mrg'],
    body: `## 50–100 MRG\n\nMake http_get failures return clear values instead of hard crash where possible.\n\n## Acceptance\n- [ ] Tests or sample demonstrating graceful fail\n${footer}`,
  },
  {
    title: '[50 MRG] Docs: .meta platform enum table keep in sync',
    labels: ['bounty', 'bounty: feature', 'docs', 'reward:50-mrg', 'good first issue'],
    body: `## 50 MRG\n\nEnsure README platform table matches CLI accepted values.\n\n## Acceptance\n- [ ] PR lists checked platforms\n${footer}`,
  },
  {
    title: '[200 MRG] E2E: self-host compiler fixpoint verified in CI',
    labels: ['bounty', 'bounty: feature', 'compiler', 'reward:200-mrg'],
    body: `## 200 MRG\n\nCI job or script that rebuilds oric.orb and checks byte identity / documented equivalent.\n\n## Acceptance\n- [ ] Script + CI or Windows+Linux notes\n${footer}`,
  },
  {
    title: '[50 MRG] Sample: crypto-safe demo using core crypto APIs',
    labels: ['bounty', 'bounty: feature', 'samples', 'reward:50-mrg'],
    body: `## 50 MRG\n\nSmall sample showing SHA-256/HMAC usage from Ori (if exposed).\n\n## Acceptance\n- [ ] Runnable sample + README\n${footer}`,
  },
  {
    title: '[25 MRG] Vietnamese README section (quickstart)',
    labels: ['bounty', 'bounty: feature', 'docs', 'reward:25-mrg', 'good first issue'],
    body: `## 25 MRG\n\n## Tiếng Việt quickstart in README.\n\n## Acceptance\n- [ ] Section present\n${footer}`,
  },
  {
    title: '[100 MRG] Chrome extension sample: options page',
    labels: ['bounty', 'bounty: feature', 'samples', 'platform', 'reward:100-mrg'],
    body: `## 100 MRG\n\nExtend chrome-extension sample with a simple options/settings UI via Ori host lines.\n\n## Acceptance\n- [ ] Screenshots load in chrome://extensions\n${footer}`,
  },
  {
    title: '[50 MRG] CLI: ori create templates (console vs window)',
    labels: ['bounty', 'bounty: feature', 'cli', 'reward:50-mrg'],
    body: `## 50 MRG\n\nFlags or prompts for ui: console vs window scaffold.\n\n## Acceptance\n- [ ] Generated project runs\n${footer}`,
  },
  {
    title: '[50 MRG] Docs: architecture diagram (orivm / oric / ori)',
    labels: ['bounty', 'bounty: feature', 'docs', 'reward:50-mrg'],
    body: `## 50 MRG\n\nMermaid or SVG diagram in docs/ARCHITECTURE.md.\n\n## Acceptance\n- [ ] Linked from README\n${footer}`,
  },
  {
    title: '[100 MRG] Bug hunt: array bounds edge cases in samples',
    labels: ['bounty', 'bounty: bug', 'vm', 'reward:100-mrg'],
    body: `## 100 MRG\n\nFind and fix at least one real bounds/error handling bug with repro.\n\n## Acceptance\n- [ ] Repro + fix + test/sample\n${footer}`,
  },
];

for (const issue of issues) {
  createIssue(issue.title, issue.body, issue.labels);
}
console.log(`Created ${issues.length} issues on ${REPO}`);
