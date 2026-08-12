# Code signing policy

vBird's Windows builds are signed under a free certificate provided by the
[SignPath Foundation](https://signpath.org/) for qualifying open-source
projects, once onboarding (tracked below) is complete. This page is vBird's
required policy disclosure under the
[SignPath Foundation open-source terms](https://signpath.org/terms.html).

## Roles

| Role | Who | Responsibility |
| --- | --- | --- |
| Author | Repository owner ([NutCMaster](https://github.com/NutCMaster)) | Writes and merges the code that gets built and signed. |
| Reviewer | Repository owner | Reviews external contributions before merge. vBird has no other maintainers at present. |
| Approver | Repository owner | Manually approves each signing request in the SignPath dashboard before a release is signed. This is a per-release action, not a one-time setting. |

As vBird gains maintainers, this table will be updated to reflect who holds
each role.

## What gets signed

Only `vbird-win64.zip` built by the project's own
[GitHub Actions workflow](.github/workflows/build.yml) from this repository's
source, triggered by a `v*` tag. Locally built or manually uploaded binaries
are never submitted for signing.

## Privacy

vBird's build and release process does not collect or transmit user data. The
application itself does not phone home, collect telemetry, or make network
requests. If that changes, this section will be updated and the change called
out in the release notes.

## Uninstalling

vBird ships as an unzipped folder with no installer, no registry entries, and
no files placed outside that folder. Deleting the folder removes it entirely.

## Credit

Code signing for this project is provided free of charge by the
[SignPath Foundation](https://signpath.org/), using infrastructure from
[SignPath.io](https://signpath.io/).

## Onboarding status

Signing is not yet active. The build pipeline
([.github/workflows/build.yml](.github/workflows/build.yml)) has a signing
step wired in that stays skipped until these steps are complete:

- [ ] Apply at [signpath.org](https://signpath.org/) and get accepted
- [ ] Create a SignPath.io project for vBird, with MFA enabled on the account
- [ ] Add "GitHub.com" as a trusted build system and install the
      [SignPath GitHub App](https://github.com/apps/signpath) on this repo
- [ ] Create a signing policy in the SignPath dashboard
- [ ] In this repo's Settings -> Secrets and variables -> Actions, add:
      secret `SIGNPATH_API_TOKEN`, and variables
      `SIGNPATH_ORGANIZATION_ID`, `SIGNPATH_PROJECT_SLUG`,
      `SIGNPATH_SIGNING_POLICY_SLUG`

Once set, every `v*` tag push builds, requests signing, and (after the
Approver's manual click in the SignPath dashboard) publishes the signed zip to
a GitHub Release automatically.
