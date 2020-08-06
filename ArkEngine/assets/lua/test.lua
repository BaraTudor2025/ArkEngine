testScript = {}

function testScript:bind()
	self.pp = getComponent(self, "PointParticles")
end

function testScript:update(dt)
	--print(self.ppspawn)
	self.pp.spawn = true
end

return testScript