# bash completion for flatspkg
# SPDX-License-Identifier: AGPL-3.0-only

_flatspkg()
{
	local cur prev
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	if [ "$prev" = "-l" ] || [ "$prev" = "--lang" ]; then
		COMPREPLY=( $(compgen -W "pt_BR es fr de en" -- "$cur") )
		return 0
	fi

	if [[ "$cur" == --lang=* ]]; then
		COMPREPLY=( $(compgen -W "pt_BR es fr de en" \
		                    -- "${cur#--lang=}") )
		COMPREPLY=( "${COMPREPLY[@]/#/--lang=}" )
		return 0
	fi

	COMPREPLY=( $(compgen -W "-u --update-cache -v --version \
	                          -l --lang -h --help" -- "$cur") )
	return 0
}

complete -F _flatspkg flatspkg