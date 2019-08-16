data/vulnspec/components.vs: data/vulnspec/components.vsi apps/vulngen/vulngen
	./apps/vulngen/vulngen < data/vulnspec/components.vsi > data/vulnspec/components.vs
