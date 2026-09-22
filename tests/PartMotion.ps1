$case = & (Join-Path $PSScriptRoot 'RivalDamage.ps1')
$case.Name = 'part_motion'
$case.Bug = 'Rival crashes must retain moving damage while body-part poses and simulation updates execute.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'joined parts follow the vehicle transform'
       Pattern = '\[part-motion\] vehicle transform delta applied'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'part simulation updates are queued'
       Pattern = '\[part-motion\] simulation update queued'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'part simulation updates reach live bodies'
       Pattern = '\[part-motion\] simulation update applied to body'; Min = 1 }
)
$case
