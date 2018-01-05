/////////////////////////////////////////////////
// Contributed by https://github.com/Wiethoofd //
/////////////////////////////////////////////////
"Resource/UI/TeamScorePanel.res"
{
	TeamScorePanel
	{
		ControlName    EditablePanel
		fieldName      TeamScorePanel
		xpos           0
		ypos           0
		wide           f0
		tall           100
		autoResize     0
		pinCorner      0
		visible        1
		enabled        1
		tabPosition    0
	}

	TeamNameBlu
	{
		ControlName              VariableLabel
		fieldName                TeamNameBlu
		xpos                     0
		ypos                     0
		zpos                     1
		wide                     200
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%blueteamname%"
		textinsetx               20
		textAlignment            east
		visible                  1
		auto_wide_tocontents     1

		fgcolor_override         HUDBlueTeam

		pin_to_sibling           TeamScoreBlu
		pin_corner_to_sibling    PIN_TOPRIGHT
		pin_to_sibling_corner    PIN_TOPLEFT
	}
	TeamNameBluShadow
	{
		ControlName              VariableLabel
		fieldName                TeamNameBluShadow
		xpos                     -1
		ypos                     -1
		wide                     200
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%blueteamname%"
		textinsetx               20
		textAlignment            east
		visible                  1
		auto_wide_tocontents     1

		fgcolor_override         Black

		pin_to_sibling           TeamNameBlu
		pin_corner_to_sibling    PIN_TOPLEFT
		pin_to_sibling_corner    PIN_TOPLEFT
	}
	TeamScoreBlu
	{
		ControlName              VariableLabel
		fieldName                TeamScoreBlu
		xpos                     0
		ypos                     0
		zpos                     1
		wide                     50
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%blueteamscore%"
		textAlignment            center
		visible                  1

		fgcolor_override         TanLight

		pin_to_sibling           CenterAnchor
		pin_corner_to_sibling    PIN_TOPRIGHT
		pin_to_sibling_corner    PIN_TOPLEFT
	}
	TeamScoreBluShadow
	{
		ControlName              VariableLabel
		fieldName                TeamScoreBluShadow
		xpos                     -1
		ypos                     -1
		wide                     50
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%blueteamscore%"
		textAlignment            center
		visible                  1

		fgcolor_override         Black

		pin_to_sibling           TeamScoreBlu
		pin_corner_to_sibling    PIN_TOPLEFT
		pin_to_sibling_corner    PIN_TOPLEFT
	}

	CenterAnchor
	{
		ControlName    EditablePanel
		fieldName      CenterAnchor
		xpos           cs-0.5
		ypos           0
		wide           100
		tall           0
		visible        1
	}

	TeamScoreRed
	{
		ControlName              VariableLabel
		fieldName                TeamScoreRed
		xpos                     0
		ypos                     0
		zpos                     1
		wide                     50
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%redteamscore%"
		textAlignment            center
		visible                  1

		fgcolor_override         TanLight

		pin_to_sibling           CenterAnchor
		pin_corner_to_sibling    PIN_TOPLEFT
		pin_to_sibling_corner    PIN_TOPRIGHT
	}
	TeamScoreRedShadow
	{
		ControlName              VariableLabel
		fieldName                TeamScoreRedShadow
		xpos                     -1
		ypos                     -1
		wide                     50
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%redteamscore%"
		textAlignment            center
		visible                  1

		fgcolor_override         Black

		pin_to_sibling           TeamScoreRed
		pin_corner_to_sibling    PIN_TOPLEFT
		pin_to_sibling_corner    PIN_TOPLEFT
	}
	TeamNameRed
	{
		ControlName              VariableLabel
		fieldName                TeamNameRed
		xpos                     0
		ypos                     0
		zpos                     1
		wide                     200
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%redteamname%"
		textinsetx               20
		textAlignment            west
		visible                  1
		auto_wide_tocontents     1

		fgcolor_override         HUDRedTeam

		pin_to_sibling           TeamScoreRed
		pin_corner_to_sibling    PIN_TOPLEFT
		pin_to_sibling_corner    PIN_TOPRIGHT
	}
	TeamNameRedShadow
	{
		ControlName              VariableLabel
		fieldName                TeamNameRedShadow
		xpos                     -1
		ypos                     -1
		wide                     200
		tall                     50
		font                     ScoreboardTeamNameNew
		labelText                "%redteamname%"
		textinsetx               20
		textAlignment            west
		visible                  1
		auto_wide_tocontents     1

		fgcolor_override         Black

		pin_to_sibling           TeamNameRed
		pin_corner_to_sibling    PIN_TOPLEFT
		pin_to_sibling_corner    PIN_TOPLEFT
	}
}