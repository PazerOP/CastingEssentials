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

	RedTeamCombined
	{
		ControlName      VariableLabel
		fieldName        RedTeamCombined
		xpos             c-95
		ypos             9
		zpos             1
		wide             100
		tall             50
		labelText        "%redteamname%: %redteamscore%"
		font             default
		textAlignment    north-west
		visible          1
		enabled          1
	}
	RedTeamCombinedShadow
	{
		ControlName         VariableLabel
		fieldName           RedTeamCombinedShadow
		xpos                c-94
		ypos                10
		zpos                1
		wide                100
		tall                50
		labelText           "%redteamname%: %redteamscore%"
		font                default
		textAlignment       north-west
		visible             1
		enabled             1
		fgcolor_override    Black
	}
	BlueTeamCombined
	{
		ControlName      VariableLabel
		fieldName        BlueTeamCombined
		xpos             c+45
		ypos             9
		zpos             1
		wide             100
		tall             50
		labelText        "%blueteamname%: %blueteamscore%"
		font             default
		textAlignment    north-west
		visible          1
		enabled          1
	}
	BlueTeamCombinedShadow
	{
		ControlName         VariableLabel
		fieldName           BlueTeamCombinedShadow
		xpos                c+46
		ypos                10
		wide                100
		tall                50
		labelText           "%blueteamname%: %blueteamscore%"
		font                default
		textAlignment       north-west
		fgcolor_override    Black
		visible             1
		enabled             1
	}
}