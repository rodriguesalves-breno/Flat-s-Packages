#compdef flatspkg
# SPDX-License-Identifier: AGPL-3.0-only

_flatspkg() {
	_arguments -s \
		'(-u --update-cache)'{-u,--update-cache}'[Force cache regeneration]' \
		'(-v --version)'{-v,--version}'[Print version and exit]' \
		'(-l --lang)'{-l,--lang}'[Force display language]:language:(pt_BR es fr de en)' \
		'(-h --help)'{-h,--help}'[Print usage and exit]'
}

_flatspkg "$@"