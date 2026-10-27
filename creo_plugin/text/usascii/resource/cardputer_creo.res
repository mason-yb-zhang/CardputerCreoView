(Dialog cardputer_creo
    (Components
        (Label       StatusLabel)
        (PushButton  EnableButton)
        (PushButton  DisableButton)
        (PushButton  Rate2Button)
        (PushButton  Rate5Button)
        (PushButton  Rate10Button)
        (PushButton  CloseButton)
    )
    (Resources
        (.Label                         "Cardputer Creo View")
        (.DialogStyle                   1)
        (.Resizeable                    False)
        (StatusLabel.Label              "Starting read-only checks")
        (EnableButton.Label             "Enable motion")
        (DisableButton.Label            "Disable motion")
        (Rate2Button.Label              "2 Hz (initial validation)")
        (Rate5Button.Label              "5 Hz")
        (Rate10Button.Label             "10 Hz")
        (CloseButton.Label              "Close")
        (.Layout
            (Grid (Rows 1 1 1 1 1 1 1) (Cols 1)
                StatusLabel
                EnableButton
                DisableButton
                Rate2Button
                Rate5Button
                Rate10Button
                CloseButton
            )
        )
    )
)
