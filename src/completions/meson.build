generate_completion = [python3, powerprofilesctl, '--print-completion']
if bashcomp.found()
  completions_dir = bashcomp.get_variable(pkgconfig: 'completionsdir',
    pkgconfig_define: bashcomp.version().version_compare('>= 2.10') ?
      ['datadir', get_option('datadir')] : ['prefix', prefix],
  )

  custom_target('bash-completion',
    output: 'powerprofilesctl',
    command: [
      generate_completion,
      'bash',
    ],
    capture: true,
    install: true,
    install_dir: completions_dir,
  )
endif

if zshcomp
  custom_target('zsh-completion',
    output: '_powerprofilesctl',
    command: [
      generate_completion,
      'zsh',
    ],
    capture: true,
    install: true,
    install_dir: get_option('zshcomp'),
  )
endif
