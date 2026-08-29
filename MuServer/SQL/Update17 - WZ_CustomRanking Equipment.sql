-- =============================================================================
--  WZ_CustomRanking - adds VALUE5 (the character's Inventory blob) to every
--  board so the ranking window can draw the ranked player with their gear.
--
--  VALUE1 = character name
--  VALUE2 = score
--  VALUE3 = class
--  VALUE4 = account level (VIP)
--  VALUE5 = Character.Inventory   <-- new
--
--  Boards 3 and 4 read their score from a ranking table and join Character for
--  the class, so the blob comes from that same join.
-- =============================================================================

ALTER PROCEDURE [dbo].[WZ_CustomRanking]
	@type		SMALLINT
As
Begin

	SET NOCOUNT ON

	IF @type = 0
	BEGIN
		SELECT TOP 20 CH.name AS VALUE1, CH.masterresetcount AS VALUE2, CH.Class AS VALUE3, MI.AccountLevel AS VALUE4, CH.Inventory AS VALUE5
		FROM Character CH JOIN MEMB_INFO MI ON CH.AccountId = MI.memb___id
		ORDER BY CH.MasterResetCount DESC, CH.ResetCount DESC, CH.cLevel DESC
	END

	IF @type = 1
	BEGIN
		SELECT TOP 20 CH.name AS VALUE1, CH.resetcount AS VALUE2, CH.Class AS VALUE3, MI.AccountLevel AS VALUE4, CH.Inventory AS VALUE5
		FROM Character CH JOIN MEMB_INFO MI ON CH.AccountId = MI.memb___id
		ORDER BY CH.ResetCount DESC, CH.cLevel DESC
	END

	IF @type = 2
	BEGIN
		SELECT TOP 20 CH.name AS VALUE1, CH.cLevel AS VALUE2, CH.Class AS VALUE3, MI.AccountLevel AS VALUE4, CH.Inventory AS VALUE5
		FROM Character CH JOIN MEMB_INFO MI ON CH.AccountId = MI.memb___id
		ORDER BY CH.cLevel DESC
	END

	IF @type = 3
	BEGIN
		SELECT TOP 20 RC.name AS VALUE1, RC.Score AS VALUE2, CH.Class AS VALUE3, MI.AccountLevel AS VALUE4, CH.Inventory AS VALUE5
		FROM RankingBloodCastle RC JOIN Character CH ON RC.name = CH.name JOIN MEMB_INFO MI ON CH.AccountId = MI.memb___id
		ORDER BY RC.Score DESC
	END

	IF @type = 4
	BEGIN
		SELECT TOP 20 RD.name AS VALUE1, RD.Score AS VALUE2, CH.Class AS VALUE3, MI.AccountLevel AS VALUE4, CH.Inventory AS VALUE5
		FROM RankingDevilSquare RD JOIN Character CH ON RD.name = CH.name JOIN MEMB_INFO MI ON CH.AccountId = MI.memb___id
		ORDER BY RD.Score DESC
	END

	IF @type = 5
	BEGIN
		SELECT TOP 20 CH.name AS VALUE1, CH.Power AS VALUE2, CH.Class AS VALUE3, MI.AccountLevel AS VALUE4, CH.Inventory AS VALUE5
		FROM Character CH JOIN MEMB_INFO MI ON CH.AccountId = MI.memb___id
		ORDER BY CH.Power DESC
	END

	SET NOCOUNT OFF

IF @@ROWCOUNT = 0
	RAISERROR ('TYPE NOT EXISTS', 50000, 1)
End
GO
