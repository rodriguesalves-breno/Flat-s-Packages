# fish completion for flatspkg
# SPDX-License-Identifier: AGPL-3.0-only

complete -c flatspkg -s u -l update-cache -d 'Force cache regeneration'
complete -c flatspkg -s v -l version     -d 'Print version and exit'
complete -c flatspkg -s l -l lang -x -a 'pt_BR es fr de en' \
                                     -d 'Force display language'
complete -c flatspkg -s h -l help        -d 'Print usage and exit'